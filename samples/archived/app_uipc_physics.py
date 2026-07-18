# -*- coding: utf-8 -*-
"""
UIPC Physics Simulation Example with RoboCute Rendering

This example demonstrates how to:
1. Use UIPC for physics simulation (affine body with external forces)
2. Use RoboCute for visualization instead of polyscope
3. Update mesh vertices dynamically based on simulation results

Based on:
- test_affine_body_external_force.py (UIPC physics)
- app_graphics_scene.py (RoboCute rendering)
"""
import os
import time
import math
from pathlib import Path
import argparse
from typing import Optional

import numpy as np

# RoboCute imports
import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
import samples.mat_builtin as mat

# UIPC imports
from uipc import Logger
from uipc import Matrix4x4
from uipc import Engine, World, Scene, SceneIO, Animation
from uipc.geometry import SimplicialComplex, SimplicialComplexIO
from uipc.geometry import label_surface, label_triangle_orient, flip_inward_triangles
from uipc import view
from uipc.constitution import AffineBodyConstitution, AffineBodyExternalBodyForce


app: rbc.app.App = None
"""Global RoboCute app singleton"""


class AssetDir:
    """Helper class to locate UIPC assets."""

    this_file = Path(os.path.dirname(__file__)).resolve()
    _assets_path = Path(this_file.parent / 'thirdparty' / 'uipc' / 'assets').resolve()
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


def create_uipc_scene() -> tuple[Scene, SceneIO, AffineBodyExternalBodyForce]:
    """
    Create and configure UIPC physics scene.
    
    Returns:
        Tuple of (Scene, SceneIO, external_force) for simulation control
    """
    Logger.set_level(Logger.Level.Info)

    # Create scene with zero gravity
    config = Scene.default_config()
    config['gravity'] = [[0], [0], [0]]
    scene = Scene(config)

    # Create constitutions
    abd = AffineBodyConstitution()
    ext_force = AffineBodyExternalBodyForce()

    # Setup contact
    scene.contact_tabular().default_model(0.5, 1e9)
    default_element = scene.contact_tabular().default_element()

    # Load cube mesh with scaling transformation
    pre_trans = Matrix4x4.Identity()
    pre_trans[0, 0] = 0.2
    pre_trans[1, 1] = 0.2
    pre_trans[2, 2] = 0.2

    io = SimplicialComplexIO(pre_trans)
    tetmesh_path = AssetDir.tetmesh_path()
    cube = io.read(f'{tetmesh_path}/cube.msh')
    cube = process_surface(cube)

    # Apply constitutions
    abd.apply_to(cube, 1e8)  # stiffness

    # Apply external force - initially zero
    initial_force = np.zeros(12)  # Vector12: [fx, fy, fz, dS/dt (9 components)]
    ext_force.apply_to(cube, initial_force)
    default_element.apply_to(cube)

    # Create object with single cube
    cube_object = scene.objects().create("cube")

    trans = Matrix4x4.Identity()
    trans[0:3, 3] = np.array([0, 0.5, 0])  # Position at y=0.5
    view(cube.transforms())[0] = trans
    cube_object.geometries().create(cube)

    # Add animator to control force
    def animate_rotating_force(info: Animation.UpdateInfo):
        """
        Apply a 12D force with both translational and rotational components.
        The cube will orbit around the origin while spinning around its own Y axis.
        """
        geo_slots = info.geo_slots()
        sim_time = info.dt() * info.frame()

        # Rotation parameters
        orbit_speed = 0.2  # rad/s - orbital motion
        spin_speed = 0.1   # rad/s - spinning motion
        force_magnitude = 0.1  # N

        # Calculate rotating force direction (in XZ plane)
        orbit_angle = orbit_speed * sim_time
        force_direction = np.array([np.cos(orbit_angle), 0.0, np.sin(orbit_angle)])
        force_3d = force_direction * force_magnitude

        # Calculate shape velocity derivative for spinning
        omega_y = spin_speed * 0.1
        shape_vel_derivative = np.array([
            0.0, 0.0, omega_y,
            0.0, 0.0, 0.0,
            -omega_y, 0.0, 0.0
        ])

        # Apply to all geometries
        for geo_slot in geo_slots:
            geo = geo_slot.geometry()
            if geo is None:
                continue

            force_attr = geo.instances().find("external_force")
            is_constrained_attr = geo.instances().find("is_constrained")

            if force_attr is None or is_constrained_attr is None:
                continue

            # Create Vector12 force
            force_v12 = np.zeros(12)
            force_v12[0:3] = force_3d
            force_v12[3:12] = shape_vel_derivative

            view(force_attr)[:] = force_v12.reshape(-1, 1)
            view(is_constrained_attr)[:] = 1

    scene.animator().insert(cube_object, animate_rotating_force)

    sio = SceneIO(scene)
    return scene, sio, ext_force


def create_visualization_mesh(
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
    mat0_json = mat.OpenPBRInterface(app._project)
    mat0_json.set_specular_roughness(0.5)
    mat0_json.set_weight_metallic(0.3)
    mat0_json.set_base_albedo((0.2, 0.6, 0.9))  # Blue-ish color

    mat0 = re.world.MaterialResource()
    mat0.load_from_json(mat0_json.dump_to_json())
    del mat0_json

    mat_vector = lc.capsule_vector()
    mat_vector.emplace_back(mat0._handle)

    # Create entity
    entity = scene.add_entity()
    entity.set_name("physics_cube")
    entity = scene.get_entity_by_name("physics_cube")

    trans = re.world.TransformComponent(entity.add_component("TransformComponent"))
    render = re.world.RenderComponent(entity.add_component("RenderComponent"))

    trans.set_pos(lc.double3(0, 0, 0), False)
    trans.set_rotation(lc.float4(0, 0, 0, 1), False)

    # Get surface dimensions from UIPC
    surface = sio.simplicial_surface()
    positions = surface.positions().view().reshape(-1, 3)
    triangles = surface.triangles().topo().view().reshape(-1, 3)
    
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
        # For now, skip update if topology changed (mesh recreation requires
        # re-calling render.update_object which we don't have access to here)
        # A full implementation would recreate the mesh and update the render component
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


class UIPCPhysicsApp:
    """
    Application that integrates UIPC physics with RoboCute rendering.
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
            workspace = str(Path(__file__).parent / "uipc_output")

        self.engine = Engine("cuda", workspace)
        self.world = World(self.engine)

        self.scene, self.sio, _ = create_uipc_scene()
        self.world.init(self.scene)

    def init_visualization(self, rbc_scene: re.world.Scene) -> None:
        """
        Initialize RoboCute visualization.
        
        Args:
            rbc_scene: RoboCute scene for rendering
        """
        if not self.sio:
            return
        self.mesh_entity, self.mesh_resource = create_visualization_mesh(
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


physics_app = UIPCPhysicsApp()


def physics_callback(ptr):
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
        description="UIPC Physics Simulation with RoboCute Rendering"
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
        required=True,
    )
    parser.add_argument(
        "-o",
        "--output",
        action="store_true",
        help="Export mode (no GUI)"
    )
    args = parser.parse_args()

    project_path = Path(args.project)

    # Initialize RoboCute app
    app = rbc.app.App()
    app.init(
        backend_name=args.backend,
        project_path=project_path,
        require_render=True
    )

    if not app.ctx:
        print("Failed to initialize context!")
        return

    # Initialize display
    app.init_display(1920, 1080, "UIPC Physics Simulation")
    if not app.display_cam:
        print("Failed to initialize display!")
        return

    # Setup camera
    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(0, 0.5, -2), False)

    app.ctx.enable_camera_control()

    if not app.scene:
        print("Scene not valid!")
        return

    # Initialize physics simulation
    print("Initializing UIPC physics simulation...")
    physics_app.init_physics()
    physics_app.init_visualization(app.scene)

    # Register callback for per-frame physics update
    data = re.world.DataComponent(
        physics_app.mesh_entity.add_component("DataComponent")
    )
    app.ctx.regist_callback("physics_update", physics_callback)
    data.bind_event(re.world.DataComponentEventType.BeforeFrame, "physics_update")

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
    tick_stage = re.world.TickStage.RasterPreview

    try:
        while not app.ctx.should_close():
            cur_time = time.time()
            delta_time = cur_time - last_time
            last_time = cur_time

            app.display_cam.set_frame_index(frame_index)

            if app.ctx.tick(delta_time, tick_stage, True):
                frame_index = 0
            else:
                frame_index += 1

    except KeyboardInterrupt:
        print("\nInterrupted by user")

    print(f"Simulation completed. Total frames: {physics_app.frame_count}")
    lc.synchronize()
    
    # Clean up objects to prevent memory leaks causing exit crashes
    # Unregister callback and clean up physics resources
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
