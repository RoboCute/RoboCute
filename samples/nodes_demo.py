"""
使用示例

展示如何直接在 Python 代码中使用 RBCNode 系统。
"""

# 导入必要的组件
import example_nodes
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
from typing import Dict, Any, List
import time


def print_header(title):
    """打印标题"""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_section(title):
    """打印小节标题"""
    print(f"\n{'─' * 70}")
    print(f"  {title}")
    print("─" * 70)


def demo():
    """运行演示"""
    print_header("🎯 RBCNode 系统演示")

    print("""
这是一个类似 ComfyUI 的节点系统实现，展示：
  ✓ 节点注册和管理
  ✓ 图构建和验证
  ✓ 拓扑排序执行
  ✓ 数据流传播
  ✓ 错误处理
    """)

    input("按回车键开始演示...")

    # 1. 显示可用节点
    print_section("📦 步骤 1: 查看可用节点")

    registry = get_registry()
    print(f"\n已注册 {len(registry)} 个节点类型:\n")

    nodes_by_category = {}
    for node_type in registry.get_all_node_types():
        metadata = registry.get_metadata(node_type)
        if metadata:
            category = metadata.category
            if category not in nodes_by_category:
                nodes_by_category[category] = []
            nodes_by_category[category].append(metadata.display_name)

    for category, nodes in sorted(nodes_by_category.items()):
        print(f"  [{category}]")
        for node in nodes:
            print(f"    • {node}")

    time.sleep(1)
    input("\n按回车继续...")

    # 2. 创建简单图
    print_section("🔧 步骤 2: 创建节点图")

    print("\n创建计算图: (10 + 5) * 2 = ?")
    print("""
    [输入:10] ──┐
                ├─→ [加法] ─→ [乘法] ─→ [输出]
    [输入:5]  ──┘              ↑
                               │
    [输入:2]  ─────────────────┘
    """)

    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="num1", node_type="input_number", inputs={"value": 10}
            ),
            NodeDefinition(
                node_id="num2", node_type="input_number", inputs={"value": 5}
            ),
            NodeDefinition(node_id="add", node_type="math_add"),
            NodeDefinition(
                node_id="num3", node_type="input_number", inputs={"value": 2}
            ),
            NodeDefinition(node_id="multiply", node_type="math_multiply"),
            NodeDefinition(node_id="output", node_type="output"),
        ],
        connections=[
            NodeConnection(
                from_node="num1", from_output="output", to_node="add", to_input="a"
            ),
            NodeConnection(
                from_node="num2", from_output="output", to_node="add", to_input="b"
            ),
            NodeConnection(
                from_node="add", from_output="result", to_node="multiply", to_input="a"
            ),
            NodeConnection(
                from_node="num3", from_output="output", to_node="multiply", to_input="b"
            ),
            NodeConnection(
                from_node="multiply",
                from_output="result",
                to_node="output",
                to_input="value",
            ),
        ],
    )

    graph = NodeGraph.from_definition(graph_def, "demo_graph")
    print(f"\n✓ 图已创建: {len(graph)} 个节点, {len(graph.get_connections())} 个连接")

    time.sleep(1)
    input("\n按回车继续...")

    # 3. 验证图
    print_section("✅ 步骤 3: 验证图")

    is_valid, error = graph.validate()
    if is_valid:
        print("\n✓ 图验证通过")

        # 显示拓扑排序
        execution_order = graph.topological_sort()
        print(f"\n执行顺序: {' → '.join(execution_order)}")
    else:
        print(f"\n✗ 图验证失败: {error}")
        return

    time.sleep(1)
    input("\n按回车继续...")

    # 4. 执行图
    print_section("🚀 步骤 4: 执行图")

    print("\n开始执行...\n")

    executor = GraphExecutor(graph)

    # 添加回调显示进度
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

    print(f"\n执行完成!")
    print(f"  状态: {result.status.value}")
    print(f"  用时: {result.duration_ms:.2f} ms")

    time.sleep(1)
    input("\n按回车查看结果...")

    # 5. 显示结果
    print_section("📊 步骤 5: 查看结果")

    print("\n各节点输出:")
    for node_id, node_result in result.node_results.items():
        if node_result.outputs:
            print(f"  • {node_id}: {node_result.outputs}")

    final_result = executor.get_node_output("output", "output")
    print(f"\n🎉 最终结果: (10 + 5) * 2 = {final_result}")

    time.sleep(1)
    input("\n按回车继续下一个演示...")

    # 6. 文本处理演示
    print_section("📝 步骤 6: 文本处理演示")

    print("\n创建文本处理图: 'Hello' + ' ' + 'RoboCute' + '!'")

    text_graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="t1", node_type="input_text", inputs={"text": "Hello"}
            ),
            NodeDefinition(
                node_id="t2", node_type="input_text", inputs={"text": "RoboCute"}
            ),
            NodeDefinition(
                node_id="concat1", node_type="text_concat", inputs={"separator": " "}
            ),
            NodeDefinition(node_id="t3", node_type="input_text", inputs={"text": "!"}),
            NodeDefinition(
                node_id="concat2", node_type="text_concat", inputs={"separator": ""}
            ),
            NodeDefinition(
                node_id="print", node_type="print", inputs={"label": "问候"}
            ),
        ],
        connections=[
            NodeConnection(
                from_node="t1",
                from_output="output",
                to_node="concat1",
                to_input="text1",
            ),
            NodeConnection(
                from_node="t2",
                from_output="output",
                to_node="concat1",
                to_input="text2",
            ),
            NodeConnection(
                from_node="concat1",
                from_output="output",
                to_node="concat2",
                to_input="text1",
            ),
            NodeConnection(
                from_node="t3",
                from_output="output",
                to_node="concat2",
                to_input="text2",
            ),
            NodeConnection(
                from_node="concat2",
                from_output="output",
                to_node="print",
                to_input="value",
            ),
        ],
    )

    text_graph = NodeGraph.from_definition(text_graph_def, "text_demo")
    text_executor = GraphExecutor(text_graph)

    print("\n执行文本处理...")
    text_result = text_executor.execute()

    if text_result.status.value == "completed":
        final_text = text_executor.get_node_output("print", "passthrough")
        print(f"\n✓ 文本处理完成: '{final_text}'")

    time.sleep(1)
    input("\n按回车查看总结...")

    # 7. 总结
    print_header("✨ 演示完成")

    print("""
本演示展示了 RBCNode 系统的核心功能：

  ✓ 节点注册和管理 (10+ 节点类型)
  ✓ 图构建和验证 (拓扑排序、循环检测)
  ✓ 数据流执行 (自动传播)
  ✓ 状态跟踪 (回调、时间统计)
  ✓ 数值和文本处理

下一步:
  • 启动 Web 服务器: python -m src.rbc_execution.main
  • 查看 API 文档: http://127.0.0.1:8000/docs
  • 运行更多示例: python src/rbc_execution/example_usage.py
  • 阅读文档: src/rbc_execution/README.md

感谢使用 RBCNode! 🚀
    """)


def example_1_simple_graph():
    """示例 1: 创建简单的数学计算图"""
    print("\n" + "=" * 60)
    print("示例 1: 简单图 - 计算 10 + 5")
    print("=" * 60)

    # 方法 1: 使用 GraphDefinition
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="num1", node_type="input_number", inputs={"value": 10}
            ),
            NodeDefinition(
                node_id="num2", node_type="input_number", inputs={"value": 5}
            ),
            NodeDefinition(node_id="add", node_type="math_add"),
        ],
        connections=[
            NodeConnection(
                from_node="num1", from_output="output", to_node="add", to_input="a"
            ),
            NodeConnection(
                from_node="num2", from_output="output", to_node="add", to_input="b"
            ),
        ],
    )

    # 从定义创建图
    graph = NodeGraph.from_definition(graph_def)

    # 执行图
    executor = GraphExecutor(graph)
    result = executor.execute()

    # 输出结果
    print(f"执行状态: {result.status.value}")
    print(f"结果: {executor.get_node_output('add', 'result')}")


def example_2_manual_graph():
    """示例 2: 手动构建图"""
    print("\n" + "=" * 60)
    print("示例 2: 手动构建图 - 计算 (20 - 5) / 3")
    print("=" * 60)

    # 创建空图
    graph = NodeGraph(graph_id="manual_graph")

    # 手动添加节点
    graph.add_node("num1", "input_number", {"value": 20})
    graph.add_node("num2", "input_number", {"value": 5})
    graph.add_node("subtract", "math_subtract")
    graph.add_node("num3", "input_number", {"value": 3})
    graph.add_node("divide", "math_divide")

    # 手动添加连接
    graph.add_connection("num1", "output", "subtract", "a")
    graph.add_connection("num2", "output", "subtract", "b")
    graph.add_connection("subtract", "result", "divide", "a")
    graph.add_connection("num3", "output", "divide", "b")

    # 验证图
    is_valid, error = graph.validate()
    print(f"图验证: {'通过' if is_valid else f'失败 - {error}'}")

    # 执行图
    executor = GraphExecutor(graph)
    result = executor.execute()

    # 输出结果
    print(f"执行状态: {result.status.value}")
    print(f"结果: {executor.get_node_output('divide', 'result')}")


def example_3_custom_node():
    """示例 3: 创建自定义节点"""
    print("\n" + "=" * 60)
    print("示例 3: 自定义节点 - 平方计算")
    print("=" * 60)

    # 定义自定义节点
    @register_node
    class SquareNode(RBCNode):
        """计算平方的节点"""

        NODE_TYPE = "custom_square"
        DISPLAY_NAME = "平方"
        CATEGORY = "自定义数学"
        DESCRIPTION = "计算输入的平方"

        @classmethod
        def get_inputs(cls) -> List[NodeInput]:
            return [NodeInput(name="value", type="number", required=True, default=0.0)]

        @classmethod
        def get_outputs(cls) -> List[NodeOutput]:
            return [NodeOutput(name="result", type="number", description="平方值")]

        def execute(self) -> Dict[str, Any]:
            value = float(self.get_input("value", 0.0))
            return {"result": value**2}

    # 使用自定义节点
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="input", node_type="input_number", inputs={"value": 7}
            ),
            NodeDefinition(node_id="square", node_type="custom_square"),
            NodeDefinition(
                node_id="output", node_type="print", inputs={"label": "7的平方"}
            ),
        ],
        connections=[
            NodeConnection(
                from_node="input",
                from_output="output",
                to_node="square",
                to_input="value",
            ),
            NodeConnection(
                from_node="square",
                from_output="result",
                to_node="output",
                to_input="value",
            ),
        ],
    )

    graph = NodeGraph.from_definition(graph_def)
    executor = GraphExecutor(graph)
    result = executor.execute()

    print(f"执行状态: {result.status.value}")


def example_4_callback():
    """示例 4: 使用执行回调"""
    print("\n" + "=" * 60)
    print("示例 4: 执行回调 - 监控执行状态")
    print("=" * 60)

    # 定义回调函数
    def execution_callback(node_id: str, status: ExecutionStatus):
        print(f"  [回调] 节点 {node_id}: {status.value}")

    # 创建图
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(node_id="a", node_type="input_number", inputs={"value": 3}),
            NodeDefinition(node_id="b", node_type="input_number", inputs={"value": 4}),
            NodeDefinition(node_id="mul", node_type="math_multiply"),
            NodeDefinition(node_id="c", node_type="input_number", inputs={"value": 5}),
            NodeDefinition(node_id="add", node_type="math_add"),
        ],
        connections=[
            NodeConnection(
                from_node="a", from_output="output", to_node="mul", to_input="a"
            ),
            NodeConnection(
                from_node="b", from_output="output", to_node="mul", to_input="b"
            ),
            NodeConnection(
                from_node="mul", from_output="result", to_node="add", to_input="a"
            ),
            NodeConnection(
                from_node="c", from_output="output", to_node="add", to_input="b"
            ),
        ],
    )

    graph = NodeGraph.from_definition(graph_def)
    executor = GraphExecutor(graph)

    # 添加回调
    executor.add_callback(execution_callback)

    # 执行
    print("\n执行图 (3 * 4 + 5 = 17):")
    result = executor.execute()

    print(f"\n最终结果: {executor.get_node_output('add', 'result')}")


def example_5_text_processing():
    """示例 5: 文本处理"""
    print("\n" + "=" * 60)
    print("示例 5: 文本处理")
    print("=" * 60)

    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="name", node_type="input_text", inputs={"text": "Alice"}
            ),
            NodeDefinition(
                node_id="greeting", node_type="input_text", inputs={"text": "Hello"}
            ),
            NodeDefinition(
                node_id="concat1", node_type="text_concat", inputs={"separator": ", "}
            ),
            NodeDefinition(
                node_id="exclaim", node_type="input_text", inputs={"text": "!"}
            ),
            NodeDefinition(
                node_id="concat2", node_type="text_concat", inputs={"separator": ""}
            ),
            NodeDefinition(
                node_id="output", node_type="print", inputs={"label": "问候语"}
            ),
        ],
        connections=[
            NodeConnection(
                from_node="greeting",
                from_output="output",
                to_node="concat1",
                to_input="text1",
            ),
            NodeConnection(
                from_node="name",
                from_output="output",
                to_node="concat1",
                to_input="text2",
            ),
            NodeConnection(
                from_node="concat1",
                from_output="output",
                to_node="concat2",
                to_input="text1",
            ),
            NodeConnection(
                from_node="exclaim",
                from_output="output",
                to_node="concat2",
                to_input="text2",
            ),
            NodeConnection(
                from_node="concat2",
                from_output="output",
                to_node="output",
                to_input="value",
            ),
        ],
    )

    graph = NodeGraph.from_definition(graph_def)
    executor = GraphExecutor(graph)
    result = executor.execute()

    print(f"\n执行状态: {result.status.value}")


def example_6_query_registry():
    """示例 6: 查询节点注册表"""
    print("\n" + "=" * 60)
    print("示例 6: 查询节点注册表")
    print("=" * 60)

    registry = get_registry()

    print(f"\n已注册节点数量: {len(registry)}")

    # 按类别分组
    nodes_by_category = {}
    for node_type in registry.get_all_node_types():
        metadata = registry.get_metadata(node_type)
        if metadata:
            category = metadata.category
            if category not in nodes_by_category:
                nodes_by_category[category] = []
            nodes_by_category[category].append((node_type, metadata.display_name))

    # 显示节点列表
    for category, nodes in sorted(nodes_by_category.items()):
        print(f"\n[{category}]")
        for node_type, display_name in nodes:
            print(f"  - {display_name} ({node_type})")

    # 查看特定节点的详细信息
    print("\n" + "-" * 60)
    print("数学加法节点详细信息:")
    print("-" * 60)
    metadata = registry.get_metadata("math_add")
    if metadata:
        print(f"类型: {metadata.node_type}")
        print(f"显示名称: {metadata.display_name}")
        print(f"类别: {metadata.category}")
        print(f"描述: {metadata.description}")
        print(f"输入:")
        for inp in metadata.inputs:
            print(f"  - {inp.name} ({inp.type}): {inp.description}")
        print(f"输出:")
        for out in metadata.outputs:
            print(f"  - {out.name} ({out.type}): {out.description}")


def main():
    """运行所有示例"""
    print("\n" + "=" * 60)
    print("RBCNode 使用示例")
    print("=" * 60)

    try:
        example_1_simple_graph()
        example_2_manual_graph()
        example_3_custom_node()
        example_4_callback()
        example_5_text_processing()
        example_6_query_registry()
        demo()

        print("\n" + "=" * 60)
        print("所有示例运行完成!")
        print("=" * 60 + "\n")

    except Exception as e:
        print(f"\n错误: {e}")
        import traceback

        traceback.print_exc()


if __name__ == "__main__":
    main()
