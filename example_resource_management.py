#!/usr/bin/env python3
"""
Example: Resource Management in RoboCute

Demonstrates the use of the resource management system for loading
meshes, textures, and materials with async loading and LOD support.
"""

import robocute as rbc
import time


def main():
    print("=== RoboCute Resource Management Example ===\n")

    # Create a scene with 2GB resource cache
    scene = rbc.Scene(cache_budget_mb=2048)
    scene.start()

    # Access the resource manager
    resource_mgr = scene.resource_manager

    print("[1] Loading basic resources...")

    # Load a mesh resource
    mesh_id = scene.load_mesh(
        "models/robot.obj",
        priority=rbc.LoadPriority.High
    )
    print(f"   Registered mesh: {mesh_id}")

    # Load a texture
    texture_id = scene.load_texture(
        "textures/metal_base.png",
        priority=rbc.LoadPriority.Normal
    )
    print(f"   Registered texture: {texture_id}")

    # Create a default material
    material_id = rbc.create_default_material(
        resource_mgr,
        name="robot_material",
        base_color=(0.8, 0.2, 0.2, 1.0),
        metallic=0.9,
        roughness=0.3
    )
    print(f"   Created material: {material_id}")

    print("\n[2] Creating entity with render component...")

    # Create an entity with transform and render components
    robot = scene.create_entity("Robot")
    scene.add_component(
        robot.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 1.0, 0.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[1.0, 1.0, 1.0]
        )
    )
    scene.add_component(
        robot.id,
        "render",
        rbc.RenderComponent(
            mesh_id=mesh_id,
            material_ids=[material_id]
        )
    )
    print(f"   Created entity: {robot.name} (ID: {robot.id})")

    print("\n[3] Batch loading resources...")

    # Batch load multiple resources
    resources = [
        {"path": "models/prop1.obj", "type": rbc.ResourceType.Mesh},
        {"path": "models/prop2.obj", "type": rbc.ResourceType.Mesh},
        {"path": "textures/ground.png", "type": rbc.ResourceType.Texture},
    ]

    batch_ids = rbc.batch_load_resources(
        resource_mgr,
        resources,
        priority=rbc.LoadPriority.Background
    )
    print(f"   Batch loaded {len(batch_ids)} resources")

    print("\n[4] Loading LOD levels...")

    # Load multiple LOD levels with priorities
    lod_ids = rbc.preload_with_lod(
        resource_mgr,
        "models/terrain.obj",
        num_levels=3
    )
    print(f"   Loaded {len(lod_ids)} LOD levels")

    print("\n[5] Using ResourceBatch utility...")

    # Use ResourceBatch helper
    batch = rbc.ResourceBatch(resource_mgr)
    batch.add("models/building1.obj", rbc.ResourceType.Mesh)
    batch.add("models/building2.obj", rbc.ResourceType.Mesh)
    batch.add("textures/brick.png", rbc.ResourceType.Texture)

    def on_batch_progress(loaded, total):
        print(f"   Batch progress: {loaded}/{total}")

    building_ids = batch.load_all(on_complete=on_batch_progress)
    print(f"   Batch submitted {len(building_ids)} resources")

    print("\n[6] Waiting for critical resources...")

    # Wait for key resources to load
    critical_resources = [mesh_id, material_id]
    if rbc.wait_for_resources(resource_mgr, critical_resources, timeout=5.0):
        print("   Critical resources loaded successfully!")
    else:
        print("   Timeout or error loading critical resources")

    print("\n[7] Checking resource states...")

    # Check resource states
    for rid in [mesh_id, texture_id, material_id]:
        metadata = resource_mgr.get_metadata(rid)
        if metadata:
            print(f"   Resource {rid}: {metadata.name} - State: {metadata.state}")

    print("\n[8] Managing memory budget...")

    # Adjust memory budget based on scene complexity
    entity_count = len(scene.get_all_entities())
    rbc.manage_memory_budget(resource_mgr, entity_count, base_budget_mb=1024)

    print("\n[9] Saving scene...")

    # Save the scene (includes resource references)
    scene.save("test_scene.rbc")
    print("   Scene saved to test_scene.rbc")

    print("\n[10] Scene simulation loop (press Ctrl+C to stop)...")

    try:
        frame = 0
        while scene.is_running() and frame < 100:
            # Update scene
            scene.update(dt=0.016)

            # Print status every 30 frames
            if frame % 30 == 0:
                mem_usage = resource_mgr.get_memory_usage()
                print(f"   Frame {frame}: Memory usage: {mem_usage / 1024 / 1024:.2f} MB")

            frame += 1
            time.sleep(0.016)

    except KeyboardInterrupt:
        print("\n   Interrupted by user")

    print("\n[11] Cleanup...")

    # Stop the scene (this stops the resource manager)
    scene.stop()

    print("\n=== Example complete ===")


def example_with_editor():
    """Example with Editor service for debugging"""
    print("\n=== Example with Editor Service ===\n")

    # Create scene
    scene = rbc.Scene()
    scene.start()

    # Start editor service
    editor_service = rbc.EditorService(scene)
    editor_service.start(port=5555)
    print("[EditorService] Started on port 5555")

    # Simulate editor connecting
    editor_service.register_editor("editor_001")

    # Load some resources
    mesh_id = scene.load_mesh("models/test.obj")

    # Broadcast resource event
    editor_service.broadcast_resource_loaded(mesh_id)

    # Simulate main loop
    print("\nRunning with editor for 5 seconds...")
    for i in range(50):
        scene.update(dt=0.1)
        editor_service.process_commands()
        editor_service.push_simulation_state()
        time.sleep(0.1)

    # Cleanup
    editor_service.stop()
    scene.stop()

    print("Editor example complete")


if __name__ == "__main__":
    main()

    # Uncomment to run editor example
    # example_with_editor()

