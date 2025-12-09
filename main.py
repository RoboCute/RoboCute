import robocute as rbc
from typing import Dict, Any, List
import time

from robocute.api import app
import custom_nodes.animation_nodes as animation_nodes

try:
    import custom_nodes.text2image_nodes as text2image_nodes

    print("    Text2Image nodes loaded")
except ImportError as e:
    print(f"    Warning: Could not load Text2Image nodes: {e}")


def init_nodes():
    """初始化节点系统"""
    print("=" * 60)
    print("RBCNode System - ComfyUI Style Node Backend")
    print("=" * 60)

    registry = rbc.get_registry()
    print(f"\nRegistered {len(registry)} node types:")

    # 按类别组织节点
    nodes_by_category = {}
    for node_type in registry.get_all_node_types():
        metadata = registry.get_metadata(node_type)
        if metadata:
            category = metadata.category
            if category not in nodes_by_category:
                nodes_by_category[category] = []
            nodes_by_category[category].append(metadata)

    # 打印节点列表
    for category, nodes in sorted(nodes_by_category.items()):
        print(f"\n  [{category}]")
        for node in nodes:
            print(f"    - {node.display_name} ({node.node_type})")

    print("\n" + "=" * 60)


def main():
    print("========== ROBOCUTE ==============")
    mesh_path = "D:/ws/data/assets/models/bunny.obj"
    scene = rbc.Scene()
    scene.start()  # start resource/scene manager singleton
    # Access the resource manager
    resource_mgr = scene.resource_manager
    print("[1] Loading basic resources...")
    # Load a mesh resource
    mesh_id = scene.load_mesh(mesh_path, priority=rbc.LoadPriority.High)
    print(f"   Registered mesh: {mesh_id}: {1 << 32 + 1}")
    print("\n[2] Creating scene entities...")
    robot = scene.create_entity("Robot")
    print(f"    Created entity: {robot.name} (ID: {robot.id})")
    # Add transform component
    scene.add_component(
        robot.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 1.0, 3.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[10.0, 10.0, 10.0],
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
    if "render" in entity_check.components:
        rc = entity_check.components["render"]
        print(f"    Render component mesh_id: {rc.mesh_id}")

    init_nodes()  # 初始化注册的节点系统

    # Start editor service
    print("\n[4] Starting Editor Service...")
    editor_service = rbc.EditorService(scene)

    # Set scene in API for animation endpoints
    print("    Setting scene in API...")
    rbc.set_scene(scene)

    # Merge Node API
    try:
        from robocute.api import app as node_api_app

        print("    Merging Node API into Editor Service...")
        editor_service._app.include_router(node_api_app.router)
        print("    Node API merged successfully")
    except ImportError as e:
        print(f"    Warning: Could not import Node API: {e}")

    editor_service.start(port=5555)
    print("    Editor Service started on port 5555")
    print("    Endpoints available:")
    print("      - GET  http://127.0.0.1:5555/scene/state")
    print("      - GET  http://127.0.0.1:5555/resources/all")
    print("      - POST http://127.0.0.1:5555/editor/register")
    print("      - GET  http://127.0.0.1:5555/nodes (Node API)")
    print("      - GET  http://127.0.0.1:5555/docs (API Documentation)")
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
