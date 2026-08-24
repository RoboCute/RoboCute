"""Generic filesystem/lock/hash primitives shared by the install path.

These helpers were historically part of ``rbc_build.shader_variants``. The
variant build/verify logic now lives in the ``rbcxx`` C++ tool; this module
only keeps the generic primitives needed by the Python packaging/install path.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import tempfile
import time
import uuid
from pathlib import Path
from typing import Any

if os.name == "nt":
    import msvcrt
else:
    import fcntl


class ShaderVariantError(RuntimeError):
    pass


class CrossProcessFileLock:
    def __init__(
        self,
        path: Path,
        *,
        timeout_seconds: float = 600.0,
        poll_seconds: float = 0.05,
    ) -> None:
        self.path = path
        self.timeout_seconds = timeout_seconds
        self.poll_seconds = poll_seconds
        self._stream: Any = None

    def __enter__(self) -> CrossProcessFileLock:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        deadline = time.monotonic() + self.timeout_seconds
        stream = None
        while stream is None:
            candidate = None
            try:
                candidate = self.path.open("a+b", buffering=0)
                candidate.seek(0, os.SEEK_END)
                if candidate.tell() == 0:
                    candidate.write(b"\0")
                stream = candidate
            except OSError as error:
                if candidate is not None:
                    candidate.close()
                if time.monotonic() >= deadline:
                    raise ShaderVariantError(
                        f"Timed out initializing shader build lock: {self.path}"
                    ) from error
                time.sleep(self.poll_seconds)
        while True:
            try:
                stream.seek(0)
                if os.name == "nt":
                    msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                self._stream = stream
                return self
            except OSError as error:
                if time.monotonic() >= deadline:
                    stream.close()
                    raise ShaderVariantError(
                        f"Timed out waiting for shader build lock: {self.path}"
                    ) from error
                time.sleep(self.poll_seconds)

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        stream = self._stream
        self._stream = None
        if stream is None:
            return
        try:
            stream.seek(0)
            if os.name == "nt":
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
        finally:
            stream.close()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    temporary.write_text(
        json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )
    os.replace(temporary, path)


def _remove_path(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def _directory_backup_path(destination: Path) -> Path:
    return destination.with_name(f".{destination.name}.backup")


def recover_directory_backup(destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    backup = _directory_backup_path(destination)
    if not backup.exists():
        return
    if destination.exists():
        _remove_path(backup)
    else:
        os.replace(backup, destination)


def _tree_identical(left: Path, right: Path) -> bool:
    """Content-based comparison of two directories (file names + digests)."""
    def collect(root: Path) -> dict[str, str]:
        if not root.is_dir():
            return {}
        return {
            path.relative_to(root).as_posix(): _sha256_file(path)
            for path in root.rglob("*")
            if path.is_file()
        }

    return collect(left) == collect(right)


def _atomic_replace_directory(staged: Path, destination: Path) -> None:
    # No-op fast path: identical trees are never rewritten (no mtime churn).
    if destination.is_dir() and _tree_identical(staged, destination):
        return
    recover_directory_backup(destination)
    backup = _directory_backup_path(destination)
    had_destination = destination.exists()
    if had_destination:
        os.replace(destination, backup)
    try:
        os.replace(staged, destination)
    except BaseException:
        if had_destination and backup.exists() and not destination.exists():
            os.replace(backup, destination)
        raise
    else:
        if backup.exists():
            try:
                _remove_path(backup)
            except OSError:
                # The new directory is already committed. Keep the backup as a
                # recovery journal and retry cleanup on the next publication.
                pass
