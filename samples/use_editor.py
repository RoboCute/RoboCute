"""
RoboCute Editor Integration Example

This example demonstrates the complete workflow of using the editor:
1. Start Python server with scene
2. Load resources
3. Create entities
4. Sync with C++ editor

Usage:
    python samples/use_editor.py
"""

import robocute as rbc
import time
from pathlib import Path


def main():
    print("=== RoboCute Editor Integration Example ===\n")

    # Step 1: Create scene
    scene = rbc.Scene(cache_budget_mb=2048)
    scene.start()

    # Step 2: Start editor service
    editor_service = rbc.EditorService(scene)
    editor_service.start(port=5555)

    # Step 3: Load mesh
    mesh_path = "D:/ws/data/assets/models/cube.obj"
    if not Path(mesh_path).exists():
        print(f"Warning: Mesh path not found: {mesh_path}")
        print("Using placeholder - editor may not display correctly")
    
    mesh_id = scene.resource_manager.load_mesh(
        mesh_path,
        priority=rbc.LoadPriority.High
    )

    # Step 4: Create entity with transform and mesh
    robot = scene.create_entity("Robot")
    scene.add_component(
        robot.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 1.0, 3.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[1.0, 1.0, 1.0],
        ),
    )
    scene.add_component(
        robot.id,
        "render",
        rbc.RenderComponent(mesh_id=mesh_id),
    )

    print(f"Scene created with entity: {robot.name}")
    print(f"Mesh resource ID: {mesh_id}")
    print(f"Editor service running on http://127.0.0.1:5555")
    print("\nStart editor.exe to visualize the scene")
    print("Press Ctrl+C to stop\n")

    # Keep running
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping...")
        editor_service.stop()
        scene.stop()
        print("Complete")


if __name__ == "__main__":
    main()

