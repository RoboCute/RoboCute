"""
Quick debug script to test graph execution with detailed logging
"""

import robocute as rbc
import sys
sys.path.insert(0, 'samples')
import animation_nodes

print("\n" + "="*70)
print("EXECUTION DEBUG TEST")
print("="*70)

# 1. Create scene
print("\n[Test] Step 1: Creating scene...")
scene = rbc.Scene()
scene.start()

# 2. Create entity
print("\n[Test] Step 2: Creating entity...")
robot = scene.create_entity("TestRobot")
scene.add_component(
    robot.id,
    "transform",
    rbc.TransformComponent(
        position=[2.0, 0.0, 0.0],
        rotation=[0.0, 0.0, 0.0, 1.0],
        scale=[1.0, 1.0, 1.0]
    )
)
print(f"[Test] ✓ Created entity: {robot.name} (ID: {robot.id})")

# 3. Check registry
print("\n[Test] Step 3: Checking node registry...")
registry = rbc.get_registry()
all_types = registry.get_all_node_types()
print(f"[Test] Registered node types: {all_types}")

required_types = ["entity_input", "rotation_animation", "animation_output"]
for node_type in required_types:
    if node_type in all_types:
        print(f"[Test] ✓ {node_type} registered")
    else:
        print(f"[Test] ✗ {node_type} NOT registered")

# 4. Set scene in API
print("\n[Test] Step 4: Setting scene in API...")
rbc.set_scene(scene)
print("[Test] ✓ Scene set in API")

# 5. Create scene context
print("\n[Test] Step 5: Creating scene context...")
scene_context = rbc.SceneContext(scene)
entities = scene_context.get_all_entities()
print(f"[Test] ✓ Scene context created with {len(entities)} entities")
for entity in entities:
    print(f"[Test]   Entity {entity.id}: {entity.name}")

# 6. Test EntityInputNode directly
print("\n[Test] Step 6: Testing EntityInputNode directly...")
try:
    node = animation_nodes.EntityInputNode("test_entity_input", scene_context)
    node.set_input("entity_id", robot.id)
    result = node.execute()
    print(f"[Test] ✓ EntityInputNode executed successfully")
    print(f"[Test]   Result: {result}")
except Exception as e:
    print(f"[Test] ✗ EntityInputNode failed: {e}")
    import traceback
    traceback.print_exc()

# 7. Build graph
print("\n[Test] Step 7: Building graph...")
graph_def = rbc.GraphDefinition(
    nodes=[
        rbc.NodeDefinition(
            node_id="entity_input",
            node_type="entity_input",
            inputs={"entity_id": robot.id}
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
                "fps": 30.0
            }
        ),
        rbc.NodeDefinition(
            node_id="anim_output",
            node_type="animation_output",
            inputs={
                "name": "test_rotation",
                "fps": 30.0
            }
        )
    ],
    connections=[
        rbc.NodeConnection(
            from_node="entity_input",
            from_output="entity",
            to_node="rotation_anim",
            to_input="entity"
        ),
        rbc.NodeConnection(
            from_node="rotation_anim",
            from_output="animation",
            to_node="anim_output",
            to_input="animation"
        )
    ]
)
print("[Test] ✓ Graph definition created")

# 8. Create graph with scene context
print("\n[Test] Step 8: Creating NodeGraph with scene context...")
try:
    graph = rbc.NodeGraph.from_definition(graph_def, "test_graph", scene_context)
    print("[Test] ✓ NodeGraph created")
except Exception as e:
    print(f"[Test] ✗ Failed to create NodeGraph: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# 9. Execute graph
print("\n[Test] Step 9: Executing graph...")
print("="*70)
try:
    executor = rbc.GraphExecutor(graph, scene_context)
    result = executor.execute()
    print("="*70)
    
    if result.status.value == "completed":
        print(f"\n[Test] ✓ Execution SUCCEEDED")
        print(f"[Test]   Duration: {result.duration_ms:.2f} ms")
    else:
        print(f"\n[Test] ✗ Execution FAILED")
        print(f"[Test]   Error: {result.error}")
        
        # Show failed nodes
        for node_id, node_result in result.node_results.items():
            if node_result.status.value == "failed":
                print(f"[Test]   Failed node: {node_id}")
                print(f"[Test]     Error: {node_result.error}")
        sys.exit(1)
        
except Exception as e:
    print("="*70)
    print(f"\n[Test] ✗ Exception during execution: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# 10. Check animation
print("\n[Test] Step 10: Checking stored animation...")
animations = scene.get_all_animations()
if "test_rotation" in animations:
    anim = animations["test_rotation"]
    print(f"[Test] ✓ Animation stored successfully")
    print(f"[Test]   Name: {anim.name}")
    print(f"[Test]   Total frames: {anim.get_total_frames()}")
    print(f"[Test]   FPS: {anim.fps}")
    print(f"[Test]   Sequences: {len(anim.sequences)}")
else:
    print(f"[Test] ✗ Animation not found in scene")
    print(f"[Test]   Available animations: {list(animations.keys())}")
    sys.exit(1)

# Cleanup
scene.stop()

print("\n" + "="*70)
print("EXECUTION DEBUG TEST PASSED ✓")
print("="*70)
print("\nIf this test passes but Editor execution fails, the issue is likely:")
print("  1. Graph serialization from Editor (check JSON format)")
print("  2. HTTP communication (check network logs)")
print("  3. API request parsing (check server logs)")

