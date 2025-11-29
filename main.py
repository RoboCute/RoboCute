import robocute as rbc
from typing import Dict, Any, List
import time


@rbc.node_registry
class RotSimNode(rbc.RBCNode):
    """RotSim Node
    最简单的自定义Node来模拟一个物理仿真过程。
    对于编辑器来说，所有的物理仿真过程实质上都是动画生成的过程，输入场景中的Entity元素，在Scene中查询必要的数据，执行算法并最终生成一个动画序列导出，同步到编辑器中进行可视化展示
    """

    NODE_TYPE = "NODETYPE_ROTSIM"
    DISPLAY_NAME = "RotSim"
    CATEGORY = "SIM"
    DESCRIPTION = "提供若干选中的场景，执行‘绕圈’这个物理仿真"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return []

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return []

    def execute(self) -> Dict[str, Any]:
        # sim start from
        # sim end frame
        # for execute sim
        # return animation sequence
        return {}


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
    if "render" in entity_check.components:
        rc = entity_check.components["render"]
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
