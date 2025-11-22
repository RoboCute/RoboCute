# RBCNode - ComfyUI Style Node System

一个类似 ComfyUI 的最简节点后端系统，支持节点注册、节点图构建、图执行和 FastAPI Web 接口。

## 功能特性

- ✅ 节点基类和注册系统
- ✅ 节点图构建和验证
- ✅ 拓扑排序和循环检测
- ✅ 图执行引擎
- ✅ FastAPI RESTful API
- ✅ 示例节点（数学运算、文本处理等）
- ✅ 执行状态跟踪
- ✅ 结果缓存

## 快速开始

### 1. 安装依赖

确保已安装所需的依赖包：

```bash
pip install fastapi uvicorn pydantic
```

或者使用项目的 pyproject.toml：

```bash
pip install -e .
```

### 2. 启动服务器

```bash
# 在项目根目录下运行
python -m src.rbc_execution.main

# 或指定端口和主机
python -m src.rbc_execution.main --host 0.0.0.0 --port 8080

# 开发模式（自动重载）
python -m src.rbc_execution.main --reload
```

服务器启动后，可以访问：
- API 文档: http://127.0.0.1:8000/docs
- 备用文档: http://127.0.0.1:8000/redoc
- 健康检查: http://127.0.0.1:8000/health

### 3. 使用 API

#### 获取所有可用节点

```bash
curl http://127.0.0.1:8000/nodes
```

#### 创建并执行节点图

示例：计算 (10 + 5) * 2

```bash
curl -X POST http://127.0.0.1:8000/graph/execute \
  -H "Content-Type: application/json" \
  -d '{
    "graph_definition": {
      "nodes": [
        {
          "node_id": "input1",
          "node_type": "input_number",
          "inputs": {"value": 10}
        },
        {
          "node_id": "input2",
          "node_type": "input_number",
          "inputs": {"value": 5}
        },
        {
          "node_id": "add",
          "node_type": "math_add",
          "inputs": {}
        },
        {
          "node_id": "input3",
          "node_type": "input_number",
          "inputs": {"value": 2}
        },
        {
          "node_id": "multiply",
          "node_type": "math_multiply",
          "inputs": {}
        },
        {
          "node_id": "output",
          "node_type": "output",
          "inputs": {}
        }
      ],
      "connections": [
        {
          "from_node": "input1",
          "from_output": "output",
          "to_node": "add",
          "to_input": "a"
        },
        {
          "from_node": "input2",
          "from_output": "output",
          "to_node": "add",
          "to_input": "b"
        },
        {
          "from_node": "add",
          "from_output": "result",
          "to_node": "multiply",
          "to_input": "a"
        },
        {
          "from_node": "input3",
          "from_output": "output",
          "to_node": "multiply",
          "to_input": "b"
        },
        {
          "from_node": "multiply",
          "from_output": "result",
          "to_node": "output",
          "to_input": "value"
        }
      ],
      "metadata": {}
    }
  }'
```

#### 查询执行结果

```bash
# 使用返回的 execution_id
curl http://127.0.0.1:8000/graph/{execution_id}/outputs
```

## 架构说明

### 核心组件

```
src/rbc_execution/
├── __init__.py          # 模块导出
├── node_base.py         # 节点基类
├── node_registry.py     # 节点注册中心
├── graph.py             # 节点图管理
├── executor.py          # 图执行引擎
├── example_nodes.py     # 示例节点
├── api.py               # FastAPI 接口
├── main.py              # 主程序入口
└── README.md            # 本文档
```

### 节点类型

已实现的示例节点：

**输入节点**
- `input_number`: 输入数值
- `input_text`: 输入文本

**数学运算**
- `math_add`: 加法
- `math_subtract`: 减法
- `math_multiply`: 乘法
- `math_divide`: 除法

**文本处理**
- `text_concat`: 文本连接

**转换**
- `number_to_text`: 数值转文本

**输出和调试**
- `output`: 输出节点
- `print`: 打印节点

## API 端点

### 节点管理

- `GET /nodes` - 获取所有节点类型
- `GET /nodes/{node_type}` - 获取指定节点的详细信息

### 图管理

- `POST /graph/create` - 创建节点图
- `POST /graph/execute` - 执行节点图
- `GET /graph/{graph_id}/status` - 查询执行状态
- `GET /graph/{graph_id}/outputs` - 获取执行输出
- `DELETE /graph/{graph_id}` - 删除图
- `GET /graphs` - 列出所有图

### 执行管理

- `GET /executions` - 列出所有执行结果
- `POST /clear` - 清空所有数据

### 系统

- `GET /` - API 信息
- `GET /health` - 健康检查

## 自定义节点

### 创建自定义节点

```python
from src.rbc_execution import RBCNode, NodeInput, NodeOutput, register_node
from typing import Dict, Any, List

@register_node
class MyCustomNode(RBCNode):
    """自定义节点"""
    
    NODE_TYPE = "my_custom_node"
    DISPLAY_NAME = "我的自定义节点"
    CATEGORY = "自定义"
    DESCRIPTION = "这是一个自定义节点示例"
    
    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(
                name="input_value",
                type="number",
                required=True,
                default=0,
                description="输入值"
            )
        ]
    
    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [
            NodeOutput(
                name="output_value",
                type="number",
                description="输出值"
            )
        ]
    
    def execute(self) -> Dict[str, Any]:
        input_value = self.get_input("input_value", 0)
        # 自定义逻辑
        result = input_value * 2 + 1
        return {"output_value": result}
```

### 在模块中注册

在 `example_nodes.py` 中添加你的节点，或创建新的模块并在 `__init__.py` 中导入。

## 编程式使用

除了通过 API，也可以直接在 Python 代码中使用：

```python
from src.rbc_execution import (
    NodeGraph, GraphDefinition, NodeDefinition, NodeConnection,
    GraphExecutor
)

# 创建图定义
graph_def = GraphDefinition(
    nodes=[
        NodeDefinition(node_id="n1", node_type="input_number", inputs={"value": 10}),
        NodeDefinition(node_id="n2", node_type="input_number", inputs={"value": 5}),
        NodeDefinition(node_id="n3", node_type="math_add"),
    ],
    connections=[
        NodeConnection(from_node="n1", from_output="output", to_node="n3", to_input="a"),
        NodeConnection(from_node="n2", from_output="output", to_node="n3", to_input="b"),
    ]
)

# 创建图
graph = NodeGraph.from_definition(graph_def)

# 执行图
executor = GraphExecutor(graph)
result = executor.execute()

# 获取结果
print(f"Status: {result.status}")
print(f"Outputs: {executor.get_all_outputs()}")
```

## 测试

运行提供的测试脚本：

```bash
python src/rbc_execution/test_example.py
```

## 注意事项

- 这是一个最简实现，用于演示节点系统的核心功能
- 执行结果存储在内存中，服务器重启后会丢失
- 没有实现持久化存储
- 没有实现真正的 ComfyUI 兼容性，仅模拟其架构
- 适合学习和原型开发

## 许可证

本项目是 RoboCute 项目的一部分。

