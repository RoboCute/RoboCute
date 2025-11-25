"""
测试示例脚本

演示如何使用 RBCNode 系统创建和执行节点图。
"""

from rbc_execution import (
    NodeGraph,
    GraphDefinition,
    NodeDefinition,
    NodeConnection,
    GraphExecutor,
    get_registry,
)


def test_simple_math():
    """测试简单数学运算：(10 + 5) * 2 = 30"""
    print("\n" + "=" * 60)
    print("Test 1: Simple Math - (10 + 5) * 2")
    print("=" * 60)

    # 创建图定义
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="input1", node_type="input_number", inputs={"value": 10}
            ),
            NodeDefinition(
                node_id="input2", node_type="input_number", inputs={"value": 5}
            ),
            NodeDefinition(node_id="add", node_type="math_add"),
            NodeDefinition(
                node_id="input3", node_type="input_number", inputs={"value": 2}
            ),
            NodeDefinition(node_id="multiply", node_type="math_multiply"),
            NodeDefinition(node_id="output", node_type="output"),
        ],
        connections=[
            NodeConnection(
                from_node="input1", from_output="output", to_node="add", to_input="a"
            ),
            NodeConnection(
                from_node="input2", from_output="output", to_node="add", to_input="b"
            ),
            NodeConnection(
                from_node="add", from_output="result", to_node="multiply", to_input="a"
            ),
            NodeConnection(
                from_node="input3",
                from_output="output",
                to_node="multiply",
                to_input="b",
            ),
            NodeConnection(
                from_node="multiply",
                from_output="result",
                to_node="output",
                to_input="value",
            ),
        ],
    )

    # 创建并执行图
    graph = NodeGraph.from_definition(graph_def, "test_math")
    executor = GraphExecutor(graph)
    result = executor.execute()

    # 显示结果
    print(f"\nExecution Status: {result.status.value}")
    print(f"Duration: {result.duration_ms:.2f}ms")
    print(f"\nNode Results:")
    for node_id, node_result in result.node_results.items():
        print(f"  {node_id}: {node_result.outputs}")

    # 验证结果
    output_value = executor.get_node_output("output", "output")
    print(f"\nFinal Output: {output_value}")
    assert output_value == 30, f"Expected 30, got {output_value}"
    print("✓ Test passed!")


def test_text_processing():
    """测试文本处理"""
    print("\n" + "=" * 60)
    print("Test 2: Text Processing")
    print("=" * 60)

    # 创建图定义
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="text1", node_type="input_text", inputs={"text": "Hello"}
            ),
            NodeDefinition(
                node_id="text2", node_type="input_text", inputs={"text": "World"}
            ),
            NodeDefinition(
                node_id="concat", node_type="text_concat", inputs={"separator": " "}
            ),
            NodeDefinition(
                node_id="print", node_type="print", inputs={"label": "Result"}
            ),
        ],
        connections=[
            NodeConnection(
                from_node="text1",
                from_output="output",
                to_node="concat",
                to_input="text1",
            ),
            NodeConnection(
                from_node="text2",
                from_output="output",
                to_node="concat",
                to_input="text2",
            ),
            NodeConnection(
                from_node="concat",
                from_output="output",
                to_node="print",
                to_input="value",
            ),
        ],
    )

    # 创建并执行图
    graph = NodeGraph.from_definition(graph_def, "test_text")
    executor = GraphExecutor(graph)
    result = executor.execute()

    # 显示结果
    print(f"\nExecution Status: {result.status.value}")
    print(f"Duration: {result.duration_ms:.2f}ms")

    # 验证结果
    output_value = executor.get_node_output("print", "passthrough")
    print(f"Final Output: {output_value}")
    assert output_value == "Hello World", (
        f"Expected 'Hello World', got '{output_value}'"
    )
    print("✓ Test passed!")


def test_number_to_text():
    """测试数值转文本"""
    print("\n" + "=" * 60)
    print("Test 3: Number to Text Conversion")
    print("=" * 60)

    # 创建图定义
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="num1", node_type="input_number", inputs={"value": 42}
            ),
            NodeDefinition(
                node_id="num2", node_type="input_number", inputs={"value": 58}
            ),
            NodeDefinition(node_id="add", node_type="math_add"),
            NodeDefinition(node_id="convert", node_type="number_to_text"),
            NodeDefinition(
                node_id="prefix", node_type="input_text", inputs={"text": "Result: "}
            ),
            NodeDefinition(node_id="concat", node_type="text_concat"),
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
                from_node="add",
                from_output="result",
                to_node="convert",
                to_input="number",
            ),
            NodeConnection(
                from_node="prefix",
                from_output="output",
                to_node="concat",
                to_input="text1",
            ),
            NodeConnection(
                from_node="convert",
                from_output="text",
                to_node="concat",
                to_input="text2",
            ),
            NodeConnection(
                from_node="concat",
                from_output="output",
                to_node="output",
                to_input="value",
            ),
        ],
    )

    # 创建并执行图
    graph = NodeGraph.from_definition(graph_def, "test_conversion")
    executor = GraphExecutor(graph)
    result = executor.execute()

    # 显示结果
    print(f"\nExecution Status: {result.status.value}")
    print(f"Duration: {result.duration_ms:.2f}ms")

    # 验证结果
    output_value = executor.get_node_output("output", "output")
    print(f"Final Output: {output_value}")
    assert output_value == "Result: 100.0", (
        f"Expected 'Result: 100.0', got '{output_value}'"
    )
    print("✓ Test passed!")


def test_error_handling():
    """测试错误处理（除以零）"""
    print("\n" + "=" * 60)
    print("Test 4: Error Handling - Division by Zero")
    print("=" * 60)

    # 创建图定义
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="num", node_type="input_number", inputs={"value": 10}
            ),
            NodeDefinition(
                node_id="zero", node_type="input_number", inputs={"value": 0}
            ),
            NodeDefinition(node_id="divide", node_type="math_divide"),
        ],
        connections=[
            NodeConnection(
                from_node="num", from_output="output", to_node="divide", to_input="a"
            ),
            NodeConnection(
                from_node="zero", from_output="output", to_node="divide", to_input="b"
            ),
        ],
    )

    # 创建并执行图
    graph = NodeGraph.from_definition(graph_def, "test_error")
    executor = GraphExecutor(graph)
    result = executor.execute()

    # 显示结果
    print(f"\nExecution Status: {result.status.value}")
    print(f"Error: {result.error}")

    # 验证结果
    assert result.status.value == "failed", "Expected execution to fail"
    print("✓ Test passed!")


def list_available_nodes():
    """列出所有可用节点"""
    print("\n" + "=" * 60)
    print("Available Nodes")
    print("=" * 60)

    registry = get_registry()

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
        print(f"\n[{category}]")
        for node in nodes:
            print(f"  {node.display_name} ({node.node_type})")
            print(f"    {node.description}")
            if node.inputs:
                print(f"    Inputs: {', '.join([i.name for i in node.inputs])}")
            if node.outputs:
                print(f"    Outputs: {', '.join([o.name for o in node.outputs])}")


def main():
    """运行所有测试"""
    print("\n" + "=" * 60)
    print("RBCNode System - Test Suite")
    print("=" * 60)

    # 列出可用节点
    list_available_nodes()

    # 运行测试
    try:
        test_simple_math()
        test_text_processing()
        test_number_to_text()
        test_error_handling()

        print("\n" + "=" * 60)
        print("All tests passed! ✓")
        print("=" * 60 + "\n")

    except Exception as e:
        print(f"\n✗ Test failed with error: {e}")
        import traceback

        traceback.print_exc()


if __name__ == "__main__":
    main()
