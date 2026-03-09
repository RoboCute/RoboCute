# -*- coding: utf-8 -*-
"""
UIPC ABD+FEM Simulation Example with RoboCute Rendering

This example demonstrates how to:
1. Use UIPC for physics simulation with both ABD (Affine Body Dynamics) 
   and FEM (Finite Element Method)
2. Use RoboCute for visualization instead of polyscope
3. Update mesh vertices dynamically based on simulation results

Based on:
- test_abd_fem.py (UIPC physics: ABD + FEM combined simulation)
- app_uipc_physics.py (RoboCute rendering integration)
"""
import os
import time
from pathlib import Path
import argparse
from typing import Optional

import numpy as np
from PIL import Image

# RoboCute imports
import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
import samples.mat_builtin as mat
from robocute.utils.rotation import euler_to_quaternion, degrees_to_radians

# UIPC imports
from uipc import Logger
from uipc import Matrix4x4
from uipc import Engine, World, Scene, SceneIO
from uipc.geometry import SimplicialComplex, SimplicialComplexIO
from uipc.geometry import label_surface, label_triangle_orient, flip_inward_triangles
from uipc.geometry import ground
from uipc import view
from uipc.constitution import StableNeoHookean, AffineBodyConstitution, ElasticModuli


app: rbc.app.App = None
"""Global RoboCute app singleton"""


class AssetDir:
    """Helper class to locate UIPC assets."""

    this_file = Path(os.path.dirname(__file__)).resolve()
    _assets_path = Path(this_file.parent / 'thirdparty' /
                        'uipc' / 'assets').resolve()
    _tetmesh_path = _assets_path / 'sim_data' / 'tetmesh'

    @staticmethod
    def tetmesh_path() -> str:
        """Return path to tetmesh directory."""
        # Fallback to a relative path if not found
        if not AssetDir._tetmesh_path.exists():
            # Try to find in libuipc location
            libuipc_path = Path("C:/dev/libuipc/assets/sim_data/tetmesh")
            if libuipc_path.exists():
                return str(libuipc_path)
        return str(AssetDir._tetmesh_path)


def process_surface(sc: SimplicialComplex) -> SimplicialComplex:
    """
    Process surface mesh for simulation.

    Args:
        sc: Input simplicial complex

    Returns:
        Processed simplicial complex with surface labels
    """
    label_surface(sc)
    label_triangle_orient(sc)
    sc = flip_inward_triangles(sc)
    return sc


def create_uipc_scene() -> tuple[Scene, SceneIO]:
    """
    Create and configure UIPC physics scene with ABD+FEM simulation.

    Returns:
        Tuple of (Scene, SceneIO) for simulation control
    """
    Logger.set_level(Logger.Level.Info)

    # Create scene with default config (gravity enabled)
    config = Scene.default_config()
    scene = Scene(config)

    # Create constitutions: FEM (StableNeoHookean) and ABD (AffineBodyConstitution)
    snk = StableNeoHookean()
    abd = AffineBodyConstitution()

    # Insert constitutions into scene
    scene.constitution_tabular().insert(snk)
    scene.constitution_tabular().insert(abd)

    # Setup contact
    scene.contact_tabular().default_model(0.5, 1e9)
    default_element = scene.contact_tabular().default_element()

    # Load cube mesh
    pre_trans = Matrix4x4.Identity()
    io = SimplicialComplexIO(pre_trans)
    tetmesh_path = AssetDir.tetmesh_path()
    cube = io.read(f'{tetmesh_path}/cube.msh')
    cube = process_surface(cube)

    # Create FEM cube with StableNeoHookean constitution
    fem_cube = cube.copy()
    moduli = ElasticModuli.youngs_poisson(1e5, 0.49)
    snk.apply_to(fem_cube, moduli)
    default_element.apply_to(fem_cube)

    # Create ABD cube with AffineBodyConstitution
    abd_cube = cube.copy()
    abd.apply_to(abd_cube, 1e8)
    default_element.apply_to(abd_cube)

    # Create object with alternating FEM and ABD cubes
    object = scene.objects().create("object")
    N = 15  # Number of cubes in the stack

    for i in range(N):
        geo = None
        if i % 2 == 0:
            # Even index: FEM cube
            geo = fem_cube.copy()
            pos_v = view(geo.positions())
            for j in range(len(pos_v)):
                pos_v[j][1] += 1.2 * i
        else:
            # Odd index: ABD cube
            geo = abd_cube.copy()
            pos_v = view(geo.positions())
            for j in range(len(pos_v)):
                pos_v[j][1] += 1.2 * i
        object.geometries().create(geo)

    # Add ground
    g = ground(-1.2)
    object.geometries().create(g)

    sio = SceneIO(scene)
    return scene, sio


def create_visualization_mesh(
    positions,
    triangles
):
    vcount = len(positions)
    tcount = len(triangles)
    # Create mesh with actual surface dimensions
    cube_mesh = re.world.MeshResource()
    submesh_offsets = np.array([0], dtype=np.uint32)
    cube_mesh.create_empty(submesh_offsets, vcount, tcount, 0, False, False)

    # Initialize mesh data buffer with actual surface data
    # Data layout: positions (vcount * 4 floats) + indices (tcount * 3 uint32s)
    mesh_array = np.ndarray(
        vcount * 4 + tcount * 3,
        dtype=np.float32,
        buffer=cube_mesh.data_buffer(),
    )

    # Fill vertex positions
    for i, pos in enumerate(positions):
        mesh_array[i * 4 + 0] = float(pos[0])
        mesh_array[i * 4 + 1] = float(pos[1])
        mesh_array[i * 4 + 2] = float(pos[2])
        mesh_array[i * 4 + 3] = 1.0  # w component

    # Fill triangle indices (as uint32 view)
    idx_view = mesh_array[vcount * 4:].view(dtype=np.uint32)
    for i, tri in enumerate(triangles):
        idx_view[i * 3 + 0] = int(tri[0])
        idx_view[i * 3 + 1] = int(tri[1])
        idx_view[i * 3 + 2] = int(tri[2])

    cube_mesh.install()
    return cube_mesh

def create_visualization_mat(
    roughness: float = 0.5,
    metallic: float = 0.3,
    color: tuple = (0.5, 0.5, 0.5)
):
    mat0_json = mat.OpenPBRInterface(app._project)
    mat0_json.set_specular_roughness(roughness)
    mat0_json.set_weight_metallic(metallic)
    mat0_json.set_base_albedo(color)  # Blue-ish color
    mat0 = re.world.MaterialResource()
    mat0.load_from_json(mat0_json.dump_to_json())
    del mat0_json
    return mat0



def create_visualization_model(
    scene: re.world.Scene,
    sio: SceneIO
) -> tuple[re.world.Entity, re.world.MeshResource]:
    """
    Create a mesh entity for visualizing the physics simulation.

    Args:
        scene: RoboCute scene
        sio: UIPC SceneIO for extracting surface dimensions

    Returns:
        Tuple of (entity, mesh_resource) for visualization
    """
    # Create material
    material = create_visualization_mat()

    mat_vector = lc.capsule_vector()
    mat_vector.emplace_back(material._handle)

    # Create entity
    entity = scene.add_entity()
    entity.set_name("abd_fem_object")
    entity = scene.get_entity_by_name("abd_fem_object")

    trans = re.world.TransformComponent(
        entity.add_component("TransformComponent"))
    render = re.world.RenderComponent(entity.add_component("RenderComponent"))

    trans.set_pos(lc.double3(0, 0, 0), False)
    trans.set_rotation(lc.float4(0, 0, 0, 1), False)

    # Get surface dimensions from UIPC
    surface = sio.simplicial_surface()
    positions = surface.positions().view().reshape(-1, 3)
    triangles = surface.triangles().topo().view().reshape(-1, 3)

    vcount = len(positions)
    tcount = len(triangles)

    print(f"Creating mesh with {vcount} vertices, {tcount} triangles")
    
    cube_mesh = create_visualization_mesh(positions, triangles)
    
    render.update_object(mat_vector, cube_mesh)

    return entity, cube_mesh


def update_mesh_from_uipc(
    mesh: re.world.MeshResource,
    sio: SceneIO
) -> None:
    """
    Update RoboCute mesh from UIPC simulation surface.

    Args:
        mesh: RoboCute mesh resource to update
        sio: UIPC SceneIO for extracting surface
    """
    if not app.ctx:
        return

    # Get simplicial surface from UIPC
    surface = sio.simplicial_surface()
    positions = surface.positions().view().reshape(-1, 3)
    triangles = surface.triangles().topo().view().reshape(-1, 3)

    # Skip update if surface has no data
    if len(positions) == 0 or len(triangles) == 0:
        return

    # Check if we need to recreate the mesh (topology changed)
    current_vcount = mesh.vertex_count()
    current_tcount = mesh.triangle_count()

    if current_vcount != len(positions) or current_tcount != len(triangles):
        # Topology changed - skip update for now
        return

    # Update vertex positions using pos_buffer
    if mesh.vertex_count() > 0:
        pos_buffer = np.ndarray(
            mesh.vertex_count() * 4,
            dtype=np.float32,
            buffer=mesh.pos_buffer()
        )
        for i, pos in enumerate(positions):
            pos_buffer[i * 4 + 0] = float(pos[0])
            pos_buffer[i * 4 + 1] = float(pos[1])
            pos_buffer[i * 4 + 2] = float(pos[2])
            pos_buffer[i * 4 + 3] = 1.0  # w component

    # Update triangle indices using indices_buffer
    if mesh.triangle_count() > 0:
        idx_buffer = np.ndarray(
            mesh.triangle_count() * 3,
            dtype=np.uint32,
            buffer=mesh.triangle_indices_buffer()
        )
        for i, tri in enumerate(triangles):
            idx_buffer[i * 3 + 0] = int(tri[0])
            idx_buffer[i * 3 + 1] = int(tri[1])
            idx_buffer[i * 3 + 2] = int(tri[2])

    # Upload to GPU
    app.ctx.upload_mesh_data(mesh)


class UIPCABDFEMApp:
    """
    Application that integrates UIPC ABD+FEM physics with RoboCute rendering.
    """

    def __init__(self):
        self.engine: Optional[Engine] = None
        self.world: Optional[World] = None
        self.scene: Optional[Scene] = None
        self.sio: Optional[SceneIO] = None
        self.mesh_entity: Optional[re.world.Entity] = None
        self.mesh_resource: Optional[re.world.MeshResource] = None
        self.is_running = False
        self.frame_count = 0

    def init_physics(self, workspace: str = "") -> None:
        """
        Initialize UIPC physics simulation.

        Args:
            workspace: Working directory for physics simulation
        """
        if not workspace:
            workspace = str(Path(__file__).parent / "uipc_abd_fem_output")

        self.engine = Engine("cuda", workspace)
        self.world = World(self.engine)

        self.scene, self.sio = create_uipc_scene()
        self.world.init(self.scene)

    def init_visualization(self, rbc_scene: re.world.Scene) -> None:
        """
        Initialize RoboCute visualization.

        Args:
            rbc_scene: RoboCute scene for rendering
        """
        if not self.sio:
            return
        self.mesh_entity, self.mesh_resource = create_visualization_model(
            rbc_scene, self.sio
        )

    def step(self) -> None:
        """Advance physics simulation by one frame."""
        if self.world and self.is_running:
            self.world.advance()
            self.world.retrieve()
            self.frame_count += 1

            # Update visualization mesh
            if self.mesh_resource and self.sio:
                update_mesh_from_uipc(self.mesh_resource, self.sio)

    def toggle_simulation(self) -> None:
        """Toggle simulation running state."""
        self.is_running = not self.is_running


physics_app = UIPCABDFEMApp()


def physics_callback():
    """
    Callback function called every frame to update physics.

    Args:
        ptr: Component pointer from RoboCute
    """
    if not app.ctx:
        return

    physics_app.step()


def main():
    """Main entry point for the application."""
    global app, physics_app

    parser = argparse.ArgumentParser(
        description="UIPC ABD+FEM Simulation with RoboCute Rendering"
    )
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "-p",
        "--project",
        type=str,
        help="rbc project path, the directory containing rbc_project.json",
        required=False,
    )
    parser.add_argument(
        "-o",
        "--output",
        action="store_true",
        help="Export mode (no GUI)"
    )
    args = parser.parse_args()
    project_path = None
    if args.project:
        project_path = Path(args.project)

    # Initialize RoboCute app
    app = rbc.app.App()
    app.init(
        backend_name=args.backend,
        project_path=project_path,
        world_path=project_path if project_path is not None else Path(
            __file__).parent,
        require_render=True
    )

    if not app.ctx:
        print("Failed to initialize context!")
        return

    # Initialize display
    app.init_display(1920, 1080, "UIPC ABD+FEM Simulation")
    if not app.display_cam:
        print("Failed to initialize display!")
        return

    # Setup camera
    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(6, 12, -15), False)
        rot = euler_to_quaternion(
            degrees_to_radians(15), degrees_to_radians(-15), 0)
        transform.set_rotation(lc.float4(rot), False)

    app.ctx.enable_camera_control()

    if not app.scene:
        print("Scene not valid!")
        app._scene = re.world.Scene()
        atmo = re.world.AtmosphereComponent(
            app._scene.add_entity().add_component('AtmosphereComponent'))
        atmo.update_data()

    # Initialize physics simulation
    print("Initializing UIPC ABD+FEM physics simulation...")
    physics_app.init_physics()
    physics_app.init_visualization(app.scene)

    # Register callback for per-frame physics update
    data = re.world.DataComponent(
        physics_app.mesh_entity.add_component("DataComponent")
    )
    # Start simulation immediately
    physics_app.is_running = True

    print("Starting main loop...")
    print("Controls:")
    print("  - Physics runs automatically")
    print("  - Use mouse to control camera")
    print("  - Close window to exit")

    # Main loop
    last_time = time.time()
    frame_index = 0
    tick_stage = re.world.TickStage.PathTracingPreview
    render_settings = app.display_cam.render_settings()
    render_settings.set_offline_spp(4)
    physics_frame = 0
    RENDER_FRAME = 1
    physics_should_step = None
    app.set_ground_plane_mode('', height=-1.2)
    try:
        while not app.ctx.should_close():
            cur_time = time.time()
            delta_time = cur_time - last_time
            last_time = cur_time

            app.display_cam.set_frame_index(frame_index)
            physics_frame += 1
            if physics_frame >= RENDER_FRAME:
                physics_frame = 0
                physics_callback()
                physics_should_step = True
            app.ctx.tick(delta_time, tick_stage, True)
            if physics_should_step:
                frame_index = 0
                physics_should_step = False
                # Do This: Export mode
                
                # Denoise, save and export image to screenshot/
                # app.ctx.denoise()
                # screenshot_dir = Path(__file__).parent / "screenshot"
                # screenshot_dir.mkdir(exist_ok=True)
                # app.ctx.save_display_image_to(
                #     str(screenshot_dir /
                #         f"frame_{physics_app.frame_count:04d}.png")
                # )
                # print(
                #     f"Saved screenshot to {screenshot_dir}/frame_{physics_app.frame_count:04d}.png")
            else:
                frame_index += 1

    except KeyboardInterrupt:
        print("\nInterrupted by user")

    print(f"Simulation completed. Total frames: {physics_app.frame_count}")
    lc.synchronize()

    # Clean up objects to prevent memory leaks causing exit crashes
    if app.ctx:
        app.ctx.unregist_callback("physics_update")

    # Clean up mesh resource
    if physics_app.mesh_resource:
        physics_app.mesh_resource = None

    # Clean up physics engine and world
    if physics_app.world:
        physics_app.world = None
    if physics_app.engine:
        physics_app.engine = None

    # Clean up scene and SceneIO
    physics_app.scene = None
    physics_app.sio = None
    physics_app.mesh_entity = None

    app = None


if __name__ == "__main__":
    main()
