import uipc
import robocute as rbc
from typing import Dict, Any, List
import time

print(uipc.__version__)
import custom_nodes
import robocute.builtin_nodes

from robocute import (
    # 核心类
    RBCNode,
    NodeInput,
    NodeOutput,
    # 注册系统
    register_node,
    get_registry,
    # 图相关
    NodeGraph,
    GraphDefinition,
    NodeDefinition,
    NodeConnection,
    # 执行器
    GraphExecutor,
    ExecutionStatus,
)
from robocute.context import SceneContext


def main():
    print("========== ROBOCUTE ==============")
    mesh_path = "D:/ws/data/assets/models/bunny.obj"
    scene = rbc.Scene()
    scene.start()  # start resource/scene manager singleton
    mesh_id = scene.load_mesh(mesh_path, priority=rbc.LoadPriority.High)
    robot1 = scene.create_entity("Robot")
    scene.add_component(
        robot1.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 0.0, 50.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[50.0, 50.0, 50.0],
        ),
    )
    render_comp = rbc.RenderComponent(
        mesh_id=mesh_id,
        material_ids=[],
    )
    scene.add_component(robot1.id, "render", render_comp)
    entity_check = scene.get_entity(robot1.id)
    robot2 = scene.create_entity("Robot")
    scene.add_component(
        robot2.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 20.0, 20.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[10.0, 10.0, 10.0],
        ),
    )
    scene.add_component(
        robot2.id,
        "render",
        rbc.RenderComponent(
            mesh_id=mesh_id,
            material_ids=[],
        ),
    )
    rbc.set_scene(scene)  # set scene context

    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="n1",
                node_type="entity_group_input",
                inputs={"entity_ids": "[1,2]"},
            ),
            NodeDefinition(
                node_id="core_sim",
                node_type="uipc_sim",
                inputs={"duration_frames": 2, "dt": 0.01, "fps": 30.0},
            ),
            NodeDefinition(
                node_id="n2", node_type="print", inputs={"label": "Input Entity Group"}
            ),
        ],
        connections=[
            NodeConnection(
                from_node="n1",
                from_output="entity_group",
                to_node="core_sim",
                to_input="entity_group",
            ),
            NodeConnection(
                from_node="core_sim",
                from_output="animation",
                to_node="n2",
                to_input="value",
            ),
        ],
    )
    context = SceneContext(scene)
    graph = NodeGraph.from_definition(graph_def, "uipc_sim_graph", context)

    is_valid, error = graph.validate()

    if is_valid:
        execution_order = graph.topological_sort()
        if execution_order is None:
            print("\n 图存在循环")
        else:
            print(f"\n执行顺序: {' → '.join(execution_order)}")
    else:
        print(f"\n✗ 图验证失败: {error}")
        return

    executor = GraphExecutor(graph)

    def progress_callback(node_id, status):
        emoji = (
            "⏳"
            if status.value == "running"
            else "✓"
            if status.value == "completed"
            else "✗"
        )
        print(f"  {emoji} 节点 '{node_id}': {status.value}")

    executor.add_callback(progress_callback)

    result = executor.execute()

    for node_id, node_result in result.node_results.items():
        if node_result.outputs:
            print(f"  • {node_id}: {node_result.outputs}")

    final_result = executor.get_node_output("output", "output")


if __name__ == "__main__":
    main()
