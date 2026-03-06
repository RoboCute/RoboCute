from typing import Optional
import robocute.rbc_ext as re
from pathlib import Path
import robocute.rbc_ext.luisa as lc
import os
import time
import numpy as np

BUILTIN_PROGRAM_PATH = Path(
    os.path.dirname(__file__) + "/rbc_ext/_C"
)  # Built-In Runtime Path


# Python-Side Application
class App:
    _ctx: Optional[re.world.RBCContext] = None
    _initialized = False
    _instance: Optional["App"] = None
    _device = None
    _project: Optional[re.world.Project] = None
    _resolution: lc.uint2 = lc.uint2(1920, 1080)
    _scene: Optional[re.world.Scene] = None
    _display_cam: Optional[re.world.CameraComponent] = None
    _last_frame_time: float
    _requires_reset: bool = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def init(self, backend_name: str, project_path: Optional[Path], world_path: Optional[Path] = None, require_render: bool = True):
        self.init_ctx()
        self.init_device(backend_name)
        lc.init()
        if require_render:
            self.init_render()
        if project_path:
            if not world_path:
                world_path = project_path / "library"
            self.init_world(world_path)
            self.init_project(project_path)
        elif world_path:
            self.init_world(world_path)
        else:
            raise Exception('world_path or project_path required.')

        self._initialized = True

    def display_image(self, dtype=float):
        display_img = self.ctx.display_image()
        assert display_img.handle() != 18446744073709551615
        return lc.Image2D.import_native(dtype, display_img)

    def init_ctx(self):
        self._ctx = re.world.RBCContext()

    def init_world(self, world_path: Path):
        if self._ctx is not None:
            self._ctx.init_world(str(world_path), str(world_path))

    def init_device(
        self, backend_name: str, program_path: Path = BUILTIN_PROGRAM_PATH
    ):
        shader_path = program_path / f"shader_build_{backend_name}"

        if self._ctx is not None:
            print(str(shader_path))
            self._ctx.init_device(backend_name, str(
                program_path), str(shader_path))

    def init_render(self):
        if self._ctx is not None:
            self._ctx.init_render()

    def init_project(self, project_path: Path):
        self._project = re.world.Project()
        self._project.init(str(project_path / "assets"))
        self._project.scan_project()
        self._scene = self._project.import_scene("test_scene.scene", "")
        self._scene.install()

    def init_display(
        self, x: int = 1920, y: int = 1080, display_title: str = "py_window"
    ):
        self._resolution = lc.uint2(x, y)
        if not self._ctx:
            return

        self._ctx.init_display(display_title, self._resolution, True, True)
        self._display_cam = self._ctx.create_display_cam()
        self._display_cam.enable_camera()
        self._last_frame_time = time.time()

    def init_transparent_display(
        self, x: int = 1920, y: int = 1080, offset_x: int = 0, offset_y: int = 0, opacity: float = 0.5, topmost: bool = True, click_through: bool = False, display_title: str = "py_window"
    ):
        self._resolution = lc.uint2(x, y)
        if not self._ctx:
            return

        self._ctx.init_transparent_display(display_title, self._resolution, lc.uint2(
            offset_x, offset_y), opacity, topmost, click_through)
        self._display_cam = self._ctx.create_display_cam()
        self._display_cam.enable_camera()
        self._last_frame_time = time.time()

    def get_display_transform(self):
        if not self._display_cam:
            return None

        return re.world.TransformComponent(
            self._display_cam.entity().get_component("TransformComponent")
        )

    @property
    def scene(self):
        return self._scene

    @property
    def ctx(self):
        return self._ctx

    @property
    def display_cam(self):
        return self._display_cam

    @property
    def last_frame_time(self):
        return self._last_frame_time

    def initialized(self):
        return self._initialized

    def run(self):
        if not self._ctx or not self._scene or not self._display_cam:
            return
        last_time = time.time()
        frame_index = 0
        tick_stage = re.world.TickStage.PathTracingPreview
        # entity = make_cube_mesh(self._scene)
        while not self._ctx.should_close():
            cur_time = time.time()
            delta_time = cur_time - last_time
            last_time = cur_time
            self._display_cam.set_frame_index(frame_index)
            if self._ctx.tick(delta_time, tick_stage, True):
                frame_index = 0
            else:
                frame_index += 1

    def upload_mesh_data(self, mesh: re.world.MeshResource):
        if self._ctx:
            self._ctx.upload_mesh_data(mesh)
