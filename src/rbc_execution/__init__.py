"""
RBC Execution - ComfyUI Style Node System

一个类似 ComfyUI 的节点系统，支持节点注册、图构建和执行。

主要组件：
- RBCNode: 节点基类
- NodeRegistry: 节点注册中心
- NodeGraph: 节点图
- GraphExecutor: 图执行器
- FastAPI API: Web 接口

使用示例：

```python
# 导入核心组件
from rbc_execution import RBCNode, register_node, NodeGraph, GraphExecutor

# 定义自定义节点
@register_node
class MyNode(RBCNode):
    NODE_TYPE = "my_node"
    DISPLAY_NAME = "My Node"
    
    @classmethod
    def get_inputs(cls):
        return [NodeInput(name="input", type="number")]
    
    @classmethod
    def get_outputs(cls):
        return [NodeOutput(name="output", type="number")]
    
    def execute(self):
        value = self.get_input("input")
        return {"output": value * 2}

# 创建和执行图
graph = NodeGraph()
graph.add_node("node1", "my_node", {"input": 42})
executor = GraphExecutor(graph)
result = executor.execute()
```

启动服务器：

```bash
python -m src.rbc_execution.main --host 0.0.0.0 --port 8000
```
"""

# 核心类导出
from .node_base import (
    RBCNode,
    NodeInput,
    NodeOutput,
    NodeMetadata
)

from .node_registry import (
    NodeRegistry,
    register_node,
    get_registry
)

from .graph import (
    NodeGraph,
    NodeConnection,
    NodeDefinition,
    GraphDefinition
)

from .executor import (
    GraphExecutor,
    ExecutionStatus,
    NodeExecutionResult,
    GraphExecutionResult,
    ExecutionCache
)

# 自动导入示例节点以触发注册
from . import example_nodes

# 版本信息
__version__ = "0.1.0"
__author__ = "RoboCute Team"

# 导出列表
__all__ = [
    # 节点基类
    "RBCNode",
    "NodeInput",
    "NodeOutput",
    "NodeMetadata",
    
    # 注册系统
    "NodeRegistry",
    "register_node",
    "get_registry",
    
    # 图相关
    "NodeGraph",
    "NodeConnection",
    "NodeDefinition",
    "GraphDefinition",
    
    # 执行器
    "GraphExecutor",
    "ExecutionStatus",
    "NodeExecutionResult",
    "GraphExecutionResult",
    "ExecutionCache",
    
    # 模块信息
    "__version__",
    "__author__",
]

