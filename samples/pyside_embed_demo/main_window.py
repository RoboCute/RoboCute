"""PySide6 main window that hosts the native LC viewport widget."""

from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Any

# shiboken6 is shipped with PySide6 and lets us wrap raw C++ QWidget pointers.
import shiboken6
from PySide6.QtCore import Qt, Slot
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QApplication,
    QDockWidget,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPlainTextEdit,
    QPushButton,
    QSlider,
    QVBoxLayout,
    QWidget,
)

# Register Windows DLL directories before importing the C++ extension.  This is
# required on Python 3.8+ because extension-module dependencies are not resolved
# through PATH.
from samples.pyside_embed_demo import _add_dll_search_paths

_add_dll_search_paths()

# The extension is built as a normal Python extension module; run_demo.py makes
# sure the build output directory is on PYTHONPATH.
import rbc_editor_py


def _default_program_path() -> str:
    """Return a reasonable default for LuisaCompute's runtime/bin directory."""
    env = os.environ.get("RBC_BUILTIN_RUNTIME")
    if env:
        return env
    try:
        import robocute
        return str(robocute.__builtin_runtime_dir__)
    except Exception:  # pragma: no cover
        pass
    # Fallback to the source-tree built-in runtime path.
    repo_root = Path(__file__).resolve().parents[2]
    return str(repo_root / "src" / "robocute" / "rbc_ext" / "_C")


class LuisaViewportWidget(QWidget):
    """PySide6 wrapper around the C++ LC ViewportWidget.

    The widget is created in C++ (via rbc_editor_py.create_viewport) and then
    wrapped back into PySide6 using shiboken6.wrapInstance.
    """

    def __init__(
        self,
        parent: QWidget | None = None,
        program_path: str | None = None,
        backend: str = "dx",
    ) -> None:
        super().__init__(parent)
        if program_path is None:
            program_path = _default_program_path()

        # Force the parent to create its native window so we can pass the
        # native handle to the C++ extension.
        if parent is not None:
            parent.winId()
        parent_wid = parent.winId() if parent is not None else 0

        self._viewport_wid = rbc_editor_py.create_viewport(
            program_path=program_path,
            backend=backend,
            parent_wid=parent_wid,
        )
        self._wrapped = shiboken6.wrapInstance(self._viewport_wid, QWidget)
        # Ensure the wrapped widget fills this placeholder.
        self._wrapped.setParent(self)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._wrapped)
        self.setLayout(layout)

    def closeEvent(self, event: Any) -> None:
        # Ask the C++ extension to release swap chain and delete the widget.
        rbc_editor_py.destroy_viewport(self._viewport_wid)
        self._viewport_wid = 0
        super().closeEvent(event)


class MainWindow(QMainWindow):
    """Demo main window: dockable debug panel + central LC viewport."""

    def __init__(
        self,
        program_path: str | None = None,
        backend: str = "dx",
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle("RoboCute PySide6 + LuisaCompute Single-Process Demo")
        self.resize(1600, 900)

        self._program_path = program_path or _default_program_path()
        self._backend = backend
        self._viewport_wid: int | None = None

        self._setup_central_viewport()
        self._setup_debug_panel()
        self._setup_status_bar()

    def _setup_central_viewport(self) -> None:
        central = QWidget(self)
        central_layout = QVBoxLayout(central)
        central_layout.setContentsMargins(4, 4, 4, 4)

        self._viewport = LuisaViewportWidget(
            parent=central,
            program_path=self._program_path,
            backend=self._backend,
        )
        self._viewport_wid = self._viewport._viewport_wid
        central_layout.addWidget(self._viewport)
        self.setCentralWidget(central)

    def _setup_debug_panel(self) -> None:
        dock = QDockWidget("Debug Panel", self)
        dock.setAllowedAreas(Qt.LeftDockWidgetArea | Qt.RightDockWidgetArea)

        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setSpacing(12)

        # Backend / program path info
        info_box = QGroupBox("Extension State")
        info_layout = QVBoxLayout(info_box)
        self._info_label = QLabel(
            f"QApplication: {rbc_editor_py.has_qapplication()}\n"
            f"Backend: {self._backend}\n"
            f"Program path: {self._program_path}"
        )
        self._info_label.setWordWrap(True)
        info_layout.addWidget(self._info_label)
        layout.addWidget(info_box)

        # Camera controls
        camera_box = QGroupBox("Camera Controls")
        camera_layout = QVBoxLayout(camera_box)

        reset_btn = QPushButton("Reset Camera")
        reset_btn.clicked.connect(self._on_reset_camera)
        camera_layout.addWidget(reset_btn)

        dist_layout = QHBoxLayout()
        dist_layout.addWidget(QLabel("Camera distance:"))
        self._dist_slider = QSlider(Qt.Horizontal)
        self._dist_slider.setRange(1, 200)
        self._dist_slider.setValue(50)
        self._dist_slider.valueChanged.connect(self._on_distance_changed)
        dist_layout.addWidget(self._dist_slider)
        camera_layout.addLayout(dist_layout)

        layout.addWidget(camera_box)

        # Visual feedback controls
        visual_box = QGroupBox("Visual Feedback (stub renderer)")
        visual_layout = QVBoxLayout(visual_box)

        colors = [
            ("Black", 0.0, 0.0, 0.0),
            ("Gray", 0.2, 0.25, 0.3),
            ("Red", 1.0, 0.0, 0.0),
            ("Green", 0.0, 1.0, 0.0),
            ("Blue", 0.0, 0.0, 1.0),
        ]
        for name, r, g, b in colors:
            btn = QPushButton(name)
            btn.clicked.connect(
                lambda checked, rr=r, gg=g, bb=b: self._on_color_clicked(rr, gg, bb)
            )
            visual_layout.addWidget(btn)

        layout.addWidget(visual_box)

        # Log output
        log_box = QGroupBox("Event Log")
        log_layout = QVBoxLayout(log_box)
        self._log = QPlainTextEdit()
        self._log.setReadOnly(True)
        log_layout.addWidget(self._log)
        layout.addWidget(log_box)

        layout.addStretch()
        dock.setWidget(panel)
        self.addDockWidget(Qt.LeftDockWidgetArea, dock)

    def _setup_status_bar(self) -> None:
        self._status_label = QLabel("Ready")
        self.statusBar().addWidget(self._status_label)

    def _log(self, message: str) -> None:  # type: ignore[no-redef]
        # The instance attribute shadows this method after _setup_debug_panel,
        # so this method only exists before setup.  We keep a no-op to satisfy
        # type checkers; runtime calls route to the QPlainTextEdit.
        pass  # pragma: no cover

    @Slot()
    def _on_reset_camera(self) -> None:
        if self._viewport_wid is not None:
            rbc_editor_py.reset_camera(self._viewport_wid)
            self._log.appendPlainText("reset_camera()")  # type: ignore[attr-defined]

    @Slot()
    def _on_distance_changed(self, value: int) -> None:
        if self._viewport_wid is not None:
            distance = value / 10.0
            rbc_editor_py.set_camera_distance(self._viewport_wid, distance)
            self._log.appendPlainText(f"set_camera_distance({distance})")  # type: ignore[attr-defined]
            self._status_label.setText(f"Camera distance: {distance:.1f}")

    @Slot()
    def _on_color_clicked(self, r: float, g: float, b: float) -> None:
        if self._viewport_wid is not None:
            rbc_editor_py.set_clear_color(self._viewport_wid, r, g, b)
            self._log.appendPlainText(f"set_clear_color({r}, {g}, {b})")  # type: ignore[attr-defined]

    def closeEvent(self, event: Any) -> None:
        # The LuisaViewportWidget.closeEvent already destroys the C++ widget;
        # this is just a safety net.
        rbc_editor_py.destroy_all_viewports()
        super().closeEvent(event)


def run_demo(
    program_path: str | None = None,
    backend: str = "dx",
    *,
    argv: list[str] | None = None,
) -> int:
    """Entry point for the interactive PySide6 demo.

    This function blocks on QApplication.exec().  For non-interactive tests,
    use the validation scripts in this package instead.
    """
    if argv is None:
        argv = sys.argv
    app = QApplication(argv)
    win = MainWindow(program_path=program_path, backend=backend)
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(run_demo())
