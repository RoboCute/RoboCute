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


def _create_visualization_mesh(
    positions,
    indices
):
    vcount = len(positions)
    tcount = len(indices)
    assert tcount % 3 == 0
    tcount = tcount // 3
    # Create mesh with actual surface dimensions
    mesh = re.world.MeshResource()
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
    _callback = None
    _tick_stage: re.world.TickStage = re.world.TickStage.PathTracingPreview
    _delta_time: float = 0
    _exit: bool = False
    _plane_entity: re.world.Entity = None

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

    def set_user_callback(self, callback):
        assert callable(callback)
        self._callback = callback

    def call_exit(self):
        self._exit = True

    def run(self, prepare_denoise: bool = False):
        if not self._ctx or not self._scene or not self._display_cam:
            return
        last_time = time.time()
        frame_index = 0
        # entity = make_cube_mesh(self._scene)
        while not self._ctx.should_close() and not self._exit:
            cur_time = time.time()
            self._delta_time = cur_time - last_time
            last_time = cur_time
            self._display_cam.set_frame_index(frame_index)
            if self._callback is not None:
                value = self._callback()
                if value is not None and not value:
                    self._requires_reset = False
                    frame_index = 0

            if self._ctx.tick(self._delta_time, self._tick_stage, prepare_denoise) or self._requires_reset:
                frame_index = 0
                self._requires_reset = False
            else:
                frame_index += 1
        self._exit = False

    def upload_mesh_data(self, mesh: re.world.MeshResource):
        if self._ctx:
            self._ctx.upload_mesh_data(mesh)

    def set_ground_plane_mode(self, mode: str, scale: float = 100, height: float = 0, material = None):
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
        mat0 = re.world.MaterialResource()
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

        trans = re.world.TransformComponent(
            entity.add_component("TransformComponent"))
        render = re.world.RenderComponent(
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
