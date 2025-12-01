"""
MVP Test Script

Tests the complete workflow of the MVP:
1. Create scene with entity
2. Build rotation animation graph programmatically
3. Execute with scene context
4. Verify animation stored in scene
5. Can be fetched via API
"""

import robocute as rbc
import time


def test_animation_workflow():
    """Test complete animation workflow"""

    print("=" * 70)
    print("MVP Animation Workflow Test")
    print("=" * 70)

    # 1. Create scene
    print("\n[1] Creating scene with entity...")
    scene = rbc.Scene()
    scene.start()

    # Create entity
    robot = scene.create_entity("TestRobot")
    scene.add_component(
        robot.id,
        "transform",
        rbc.TransformComponent(
            position=[2.0, 0.0, 0.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[1.0, 1.0, 1.0],
        ),
    )
    print(f"   Created entity: {robot.name} (ID: {robot.id})")

    # 2. Import animation nodes
    print("\n[2] Importing animation nodes...")
    import sys

    sys.path.insert(0, "samples")
    import custom_nodes.animation_nodes as animation_nodes

    print("   Animation nodes imported")

    # 3. Build node graph
    print("\n[3] Building rotation animation graph...")
    graph_def = rbc.GraphDefinition(
        nodes=[
            rbc.NodeDefinition(
                node_id="entity_input",
                node_type="entity_input",
                inputs={"entity_id": robot.id},
            ),
            rbc.NodeDefinition(
                node_id="rotation_anim",
                node_type="rotation_animation",
                inputs={
                    "center_x": 0.0,
                    "center_y": 0.0,
                    "center_z": 0.0,
                    "radius": 2.0,
                    "angular_velocity": 1.0,
                    "duration_frames": 60,
                    "fps": 30.0,
                },
            ),
            rbc.NodeDefinition(
                node_id="anim_output",
                node_type="animation_output",
                inputs={"name": "test_rotation", "fps": 30.0},
            ),
        ],
        connections=[
            rbc.NodeConnection(
                from_node="entity_input",
                from_output="entity",
                to_node="rotation_anim",
                to_input="entity",
            ),
            rbc.NodeConnection(
                from_node="rotation_anim",
                from_output="animation",
                to_node="anim_output",
                to_input="animation",
            ),
        ],
    )
    print("   Graph created with 3 nodes, 2 connections")

    # 4. Execute graph with scene context
    print("\n[4] Executing graph with scene context...")
    scene_context = rbc.SceneContext(scene)
    graph = rbc.NodeGraph.from_definition(graph_def, "test_graph", scene_context)
    executor = rbc.GraphExecutor(graph, scene_context)
    result = executor.execute()

    if result.status.value == "completed":
        print(f"   ✓ Graph executed successfully")
        print(f"   Duration: {result.duration_ms:.2f} ms")
    else:
        print(f"   ✗ Graph execution failed: {result.error}")
        return False

    # 5. Verify animation stored in scene
    print("\n[5] Verifying animation in scene...")
    animations = scene.get_all_animations()

    if "test_rotation" not in animations:
        print("   ✗ Animation not found in scene")
        return False

    anim = animations["test_rotation"]
    print(f"   ✓ Animation found: {anim.name}")
    print(f"     Total frames: {anim.get_total_frames()}")
    print(f"     FPS: {anim.fps}")
    print(f"     Duration: {anim.get_duration_seconds():.2f}s")
    print(f"     Sequences: {len(anim.sequences)}")

    # 6. Test API access
    print("\n[6] Testing API access...")
    rbc.set_scene(scene)

    # Start a simple HTTP server to test API
    from robocute.api import app
    import threading
    import uvicorn

    def run_server():
        uvicorn.run(app, host="127.0.0.1", port=8888, log_level="error")

    server_thread = threading.Thread(target=run_server, daemon=True)
    server_thread.start()
    time.sleep(1)  # Wait for server to start

    # Test animation endpoint
    import requests

    try:
        response = requests.get("http://127.0.0.1:8888/animations")
        if response.status_code == 200:
            data = response.json()
            print(f"   ✓ API accessible")
            print(f"     Animations count: {data['count']}")
        else:
            print(f"   ✗ API returned error: {response.status_code}")
            return False
    except Exception as e:
        print(f"   ✗ API request failed: {e}")
        return False

    # Cleanup
    scene.stop()

    print("\n" + "=" * 70)
    print("MVP Test PASSED ✓")
    print("=" * 70)

    return True


if __name__ == "__main__":
    try:
        success = test_animation_workflow()
        exit(0 if success else 1)
    except Exception as e:
        print(f"\nTest FAILED with exception: {e}")
        import traceback

        traceback.print_exc()
        exit(1)
