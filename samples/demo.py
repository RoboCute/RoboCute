"""
快速演示脚本

展示 RBCNode 系统的基本功能。
"""

import time
from robocute import (
    NodeGraph,
    GraphDefinition,
    NodeDefinition,
    NodeConnection,
    GraphExecutor,
    get_registry,
)


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


if __name__ == "__main__":
    try:
        demo()
    except KeyboardInterrupt:
        print("\n\n演示已取消。")
    except Exception as e:
        print(f"\n\n错误: {e}")
        import traceback

        traceback.print_exc()
