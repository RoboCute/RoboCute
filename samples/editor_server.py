"""
RoboCute Editor Server Example

This script demonstrates how to start a Python server that can sync
scene data with a C++ editor via HTTP REST API.

Usage:
    python samples/editor_server.py

The editor will be available at http://127.0.0.1:5555
"""

import robocute as rbc
import time
import sys
from pathlib import Path


def on_mesh_complete(resource_id, res):
    """Callback when mesh loading is complete"""
    print(f"[Server] Mesh loaded: Resource ID {resource_id}")


def main():
    print("=" * 60)
    print("RoboCute Editor Server Example")
    print("=" * 60)
    print()

    # Create scene
    print("[1] Creating scene...")
    scene = rbc.Scene(cache_budget_mb=2048)
    scene.start()
    print("    Scene created and started")

    # Load mesh resource
    print("\n[2] Loading mesh resource...")
    # Use a mesh path - you should replace this with your actual mesh path
    mesh_path = "D:/ws/data/assets/models/cube.obj"
    
    # Check if default path exists, otherwise use relative path
    if not Path(mesh_path).exists():
        # Try to find a mesh in the project
        mesh_path = str(Path(__file__).parent.parent / "samples" / "test_mesh.obj")
        print(f"    Default mesh not found, using: {mesh_path}")
        print(f"    NOTE: You may need to provide a valid mesh path")
    
    mesh_id = scene.resource_manager.load_mesh(
        mesh_path,
        priority=rbc.LoadPriority.High,
        on_complete=on_mesh_complete,
    )
    print(f"    Mesh registered with ID: {mesh_id}")

    # Create entity with transform and mesh renderer
    print("\n[3] Creating scene entities...")
    robot = scene.create_entity("Robot")
    print(f"    Created entity: {robot.name} (ID: {robot.id})")

    # Add transform component
    scene.add_component(
        robot.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 1.0, 3.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[1.0, 1.0, 1.0],
        ),
    )
    print("    Added transform component")

    # Add render component with mesh reference
    render_comp = rbc.RenderComponent(
        mesh_id=mesh_id,
        material_ids=[],
    )
    print(f"    Creating render component: mesh_id={render_comp.mesh_id}")
    scene.add_component(robot.id, "render", render_comp)
    print("    Added render component")
    
    # Verify component was added
    entity_check = scene.get_entity(robot.id)
    print(f"    Entity has components: {list(entity_check.components.keys())}")
    if 'render' in entity_check.components:
        rc = entity_check.components['render']
        print(f"    Render component mesh_id: {rc.mesh_id}")

    # Start editor service
    print("\n[4] Starting Editor Service...")
    editor_service = rbc.EditorService(scene)
    editor_service.start(port=5555)
    print("    Editor Service started on port 5555")
    print("    Endpoints available:")
    print("      - GET  http://127.0.0.1:5555/scene/state")
    print("      - GET  http://127.0.0.1:5555/resources/all")
    print("      - POST http://127.0.0.1:5555/editor/register")

    print("\n[5] Server is running...")
    print("    You can now start the C++ editor (editor.exe)")
    print("    The editor will connect and display the scene")
    print("    Press Ctrl+C to stop the server")
    print()

    # Keep server running
    try:
        frame = 0
        while True:
            time.sleep(0.1)
            
            # Every 5 seconds, print status
            if frame % 50 == 0:
                editors = editor_service.get_connected_editors()
                if editors:
                    print(f"[Status] Connected editors: {', '.join(editors)}")
                else:
                    print("[Status] Waiting for editor connection...")
            
            frame += 1
            
    except KeyboardInterrupt:
        print("\n\n[Shutdown] Stopping server...")
        editor_service.stop()
        scene.stop()
        print("[Shutdown] Server stopped")
        print()

    print("=" * 60)
    print("Server Complete")
    print("=" * 60)


if __name__ == "__main__":
    main()

