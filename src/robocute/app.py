from typing import Optional, Callable
import robocute.rbc_ext as rbce
from pathlib import Path
import robocute.rbc_ext.luisa as lc
import os
import time
import numpy as np

BUILTIN_PROGRAM_PATH = Path(
    os.path.dirname(__file__) + "/rbc_ext/_C"
)  # Built-In Runtime Path

def _create_visualization_mesh(
    positions: list[tuple[float, float, float]],
    indices: list[int]
) -> rbce.world.MeshResource:
    vcount = len(positions)
    tcount = len(indices)
    assert tcount % 3 == 0
    tcount = tcount // 3
    # Create mesh with actual surface dimensions
    mesh = rbce.world.MeshResource()
    submesh_offsets = np.array([0], dtype=np.uint32)
    mesh.create_empty(submesh_offsets, vcount,
                      tcount, 0, False, False)

    # Initialize mesh data buffer with actual surface data
    # Data layout: positions (vcount * 4 floats) + indices (tcount * 3 uint32s)
    mesh_array = np.ndarray(
        vcount * 4 + tcount * 3,
        dtype=np.float32,
        buffer=mesh.data_buffer(),
    )

    # Fill vertex positions
    for i in range(len(positions)):
        pos = positions[i]
        mesh_array[i * 4 + 0] = float(pos[0])
        mesh_array[i * 4 + 1] = float(pos[1])
        mesh_array[i * 4 + 2] = float(pos[2])
        mesh_array[i * 4 + 3] = 1.0  # w component

    # Fill triangle indices (as uint32 view)
    idx_view = mesh_array[vcount * 4:].view(dtype=np.uint32)
    for i in range(len(indices)):
        idx_view[i] = int(indices[i])

    mesh.install()
    return mesh

# Python-Side Application


class App:
    _ctx: Optional[rbce.world.RBCContext] = None
    _initialized = False
    _instance: Optional["App"] = None
    _device = None
    _project: Optional[rbce.world.Project] = None
    _resolution: lc.uint2 = lc.uint2(1920, 1080)
    _window_created: lc.uint2 = lc.uint2(1920, 1080)
    _scene: Optional[rbce.world.Scene] = None
    _display_cam: Optional[rbce.world.CameraComponent] = None
    _last_frame_time: float
    _requires_reset: bool = False
    _callback = None
    _tick_stage: rbce.world.TickStage = rbce.world.TickStage.PathTracingPreview
    _delta_time: float = 0
    _exit: bool = False
    _plane_entity: rbce.world.Entity = None
    frame_index = 0
    real_frame_index = 0

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def init(self, backend_name: str, project_path: Optional[Path], world_path: Optional[Path] = None, require_render: bool = True) -> None:
        self.init_ctx()
        self.init_device(backend_name)
        lc.init()
        print(">>>>>> Device init done")
        if require_render:
            self.init_render()
            print(">>>>>> Render init done")

        if project_path:
            if not world_path:
                world_path = project_path / "library"
            print(">>>> using world_path: ", world_path)
            self.init_world(world_path)
            print(">>>> init world done")
            self.init_project(project_path)
            print(">>>> init project done")
        elif world_path:
            self.init_world(world_path)
        else:
            raise Exception('world_path or project_path required.')

        print(">>>>>> rbc app init done")
        self._initialized = True

    def display_image(self, dtype=float) -> None:
        display_img = self.ctx.display_image()
        assert display_img.handle() != 18446744073709551615
        return lc.Image2D.import_native(dtype, display_img)

    def init_ctx(self) -> None:
        self._ctx = rbce.world.RBCContext()

    def init_world(self, world_path: Path) -> None:
        if self._ctx is not None:
            self._ctx.init_world(str(world_path), str(world_path))

    def init_device(
        self, backend_name: str, program_path: Path = BUILTIN_PROGRAM_PATH
    ) -> None:
        shader_path = program_path / f"shader_build_{backend_name}"

        if self._ctx is not None:
            print(str(shader_path))
            self._ctx.init_device(backend_name, str(
                program_path), str(shader_path), False)

    def init_render(self) -> None:
        if self._ctx is not None:
            self._ctx.init_render()

    def init_project(self, project_path: Path) -> None:
        if not project_path.exists():
            raise Exception(f'Project path: {project_path} not exists.')
        self._project = rbce.world.Project()
        self._project.init(str(project_path / "assets"))
        print(f"init project {project_path} done, start scanning")
        self._project.scan_project()
        self._scene = self._project.import_scene("test_scene.scene", "")
        print("scene imported, start install ...")
        self._scene.install()

    def init_display(
        self, x: int = 1920, y: int = 1080, display_title: str = "py_window",
        create_window: bool = True, window_resizable: bool = True, full_screen: bool = False, transparent: bool = False
    ) -> None:
        self._resolution = lc.uint2(x, y)
        if not self._ctx:
            return
        self._window_created = create_window
        self._ctx.init_display(
            display_title, self._resolution, create_window, window_resizable, full_screen, transparent)
        self._display_cam = self._ctx.create_display_cam()
        self._display_cam.enable_camera()
        self._last_frame_time = time.time()

    def get_display_transform(self) -> None:
        if not self._display_cam:
            return None

        return rbce.world.TransformComponent(
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

    def set_user_callback(self, callback: Callable[[], bool | None]) -> None:
        self._callback = callback

    def call_exit(self) -> None:
        self._exit = True

    def run(self, prepare_denoise: bool = False, limit_frame: int | None = None) -> None:
        if not self._ctx or not self._scene or not self._display_cam:
            return
        last_time = time.time()

        # entity = make_cube_mesh(self._scene)

        def should_quit():
            if self._ctx.should_close() or self._exit:
                return True
            return limit_frame is not None and self.real_frame_index >= limit_frame
        
        while not should_quit():
            self.real_frame_index += 1
            cur_time = time.time()
            self._delta_time = cur_time - last_time
            last_time = cur_time
            self._display_cam.set_frame_index(self.frame_index)
            if self._callback is not None:
                value = self._callback()
                if value is not None and not value:
                    self._requires_reset = False
                    self.frame_index = 0

            if self._ctx.tick(self._delta_time, self._tick_stage, prepare_denoise) or self._requires_reset:
                self.frame_index = 0
                self._requires_reset = False
            else:
                self.frame_index += 1
        self._exit = False

    def upload_mesh_data(self, mesh: rbce.world.MeshResource) -> None:
        if self._ctx:
            self._ctx.upload_mesh_data(mesh)

    def set_ground_plane_mode(self, mode: str | None, scale: float = 100, height: float = 0, material=None) -> None:
        import samples.mat_builtin as mat
        if mode is None or mode == 'none':
            if self._plane_entity:
                del self._plane_entity
                self._plane_entity = None
            return
        if self._plane_entity:
            return
        if material and type(material) == mat.OpenPBRInterface:
            mat0_json = material
        else:
            mat0_json = mat.OpenPBRInterface(self._project)
            mat0_json.set_specular_roughness(0.5)
            mat0_json.set_weight_metallic(0.3)
            mat0_json.set_base_albedo((0.8, 0.8, 0.8))  # Blue-ish color
        mat0 = rbce.world.MaterialResource()
        mat0.load_from_json(mat0_json.dump_to_json())
        mat_vector = lc.capsule_vector()
        mat_vector.emplace_back(mat0._handle)
        scene = self.scene
        if not scene:
            print('app.scene not initialized first.')
            exit(1)
        entity = scene.add_entity()
        self._plane_entity = entity
        entity.set_name("__app_plane_entity")

        trans = rbce.world.TransformComponent(
            entity.add_component("TransformComponent"))
        render = rbce.world.RenderComponent(
            entity.add_component("RenderComponent"))

        trans.set_pos(lc.double3(0, 0, 0), False)
        trans.set_rotation(lc.float4(0, 0, 0, 1), False)
        mesh = _create_visualization_mesh(
            [
                (-scale, height, -scale),
                (scale, height, -scale),
                (-scale, height, scale),
                (scale, height, scale)
            ],
            [
                0, 1, 2,
                1, 3, 2
            ]
        )
        render.update_object(mat_vector, mesh)
