# 快速开始指南

这是一个最简化的类似 ComfyUI 的节点系统实现，包含完整的后端功能。

## 目录结构

```
src/rbc_execution/
├── __init__.py              # 模块导出
├── node_base.py             # 节点基类定义
├── node_registry.py         # 节点注册系统
├── graph.py                 # 节点图管理
├── executor.py              # 图执行引擎
├── example_nodes.py         # 示例节点实现
├── api.py                   # FastAPI Web 接口
├── main.py                  # 主程序入口
├── caching.py               # (原有文件，已空)
├── README.md                # 详细文档
├── QUICKSTART.md            # 本文件
├── example_usage.py         # Python 使用示例
├── test_example.py          # 测试脚本
├── test_api.sh              # API 测试脚本 (Bash)
└── test_api.ps1             # API 测试脚本 (PowerShell)
```

## 安装依赖

在项目根目录运行：

```bash
pip install -e .
```

或者直接安装所需的包：

```bash
pip install fastapi uvicorn pydantic
```

## 启动服务器

### 方法 1: 使用默认设置

```bash
python -m src.rbc_execution.main
```

服务器将在 http://127.0.0.1:8000 启动。

### 方法 2: 自定义配置

```bash
# 指定端口
python -m src.rbc_execution.main --port 8080

# 指定主机（允许外部访问）
python -m src.rbc_execution.main --host 0.0.0.0

# 开发模式（自动重载）
python -m src.rbc_execution.main --reload
```

## 访问 API 文档

启动服务器后，在浏览器中打开：

- **Swagger UI**: http://127.0.0.1:8000/docs
- **ReDoc**: http://127.0.0.1:8000/redoc

## 快速测试

### 1. 健康检查

```bash
curl http://127.0.0.1:8000/health
```

### 2. 查看可用节点

```bash
curl http://127.0.0.1:8000/nodes
```

### 3. 执行简单计算 (10 + 5)

**Windows (PowerShell):**

```powershell
$body = @{
    graph_definition = @{
        nodes = @(
            @{ node_id = "n1"; node_type = "input_number"; inputs = @{ value = 10 } }
            @{ node_id = "n2"; node_type = "input_number"; inputs = @{ value = 5 } }
            @{ node_id = "add"; node_type = "math_add"; inputs = @{} }
        )
        connections = @(
            @{ from_node = "n1"; from_output = "output"; to_node = "add"; to_input = "a" }
            @{ from_node = "n2"; from_output = "output"; to_node = "add"; to_input = "b" }
        )
        metadata = @{}
    }
} | ConvertTo-Json -Depth 10

Invoke-RestMethod -Uri "http://127.0.0.1:8000/graph/execute" -Method Post `
    -ContentType "application/json" -Body $body
```

**Linux/Mac (curl):**

```bash
curl -X POST http://127.0.0.1:8000/graph/execute \
  -H "Content-Type: application/json" \
  -d '{
    "graph_definition": {
      "nodes": [
        {"node_id": "n1", "node_type": "input_number", "inputs": {"value": 10}},
        {"node_id": "n2", "node_type": "input_number", "inputs": {"value": 5}},
        {"node_id": "add", "node_type": "math_add", "inputs": {}}
      ],
      "connections": [
        {"from_node": "n1", "from_output": "output", "to_node": "add", "to_input": "a"},
        {"from_node": "n2", "from_output": "output", "to_node": "add", "to_input": "b"}
      ],
      "metadata": {}
    }
  }'
```

## Python 编程使用

不使用 HTTP API，直接在 Python 代码中使用：

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
        NodeDefinition(node_id="add", node_type="math_add"),
    ],
    connections=[
        NodeConnection(from_node="n1", from_output="output", to_node="add", to_input="a"),
        NodeConnection(from_node="n2", from_output="output", to_node="add", to_input="b"),
    ]
)

# 创建并执行图
graph = NodeGraph.from_definition(graph_def)
executor = GraphExecutor(graph)
result = executor.execute()

# 获取结果
print(f"Status: {result.status}")
print(f"Result: {executor.get_node_output('add', 'result')}")
```

## 运行示例

### Python 示例

```bash
python src/rbc_execution/example_usage.py
```

这将运行多个示例，展示：
1. 简单图创建
2. 手动构建图
3. 自定义节点
4. 执行回调
5. 文本处理
6. 查询注册表

### 测试脚本

```bash
python src/rbc_execution/test_example.py
```

这将运行自动化测试，验证系统功能。

### API 测试

**Windows:**

```powershell
.\src\rbc_execution\test_api.ps1
```

**Linux/Mac:**

```bash
bash src/rbc_execution/test_api.sh
```

## 可用节点类型

系统提供以下示例节点：

### 输入节点
- `input_number`: 数值输入
- `input_text`: 文本输入

### 数学运算
- `math_add`: 加法 (a + b)
- `math_subtract`: 减法 (a - b)
- `math_multiply`: 乘法 (a * b)
- `math_divide`: 除法 (a / b)

### 文本处理
- `text_concat`: 文本连接

### 转换
- `number_to_text`: 数值转文本

### 输出和调试
- `output`: 输出节点
- `print`: 打印节点

## 创建自定义节点

```python
from src.rbc_execution import RBCNode, NodeInput, NodeOutput, register_node
from typing import Dict, Any, List

@register_node
class MyNode(RBCNode):
    NODE_TYPE = "my_node"
    DISPLAY_NAME = "我的节点"
    CATEGORY = "自定义"
    DESCRIPTION = "自定义节点描述"
    
    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [
            NodeInput(name="input", type="number", required=True)
        ]
    
    @classmethod
    def get_outputs(cls) -> List[NodeOutput]:
        return [
            NodeOutput(name="output", type="number")
        ]
    
    def execute(self) -> Dict[str, Any]:
        value = self.get_input("input")
        result = value * 2  # 自定义逻辑
        return {"output": result}
```

将自定义节点添加到 `example_nodes.py` 或创建新文件并在 `__init__.py` 中导入。

## API 端点摘要

| 方法 | 端点 | 描述 |
|------|------|------|
| GET | `/` | API 信息 |
| GET | `/health` | 健康检查 |
| GET | `/nodes` | 获取所有节点类型 |
| GET | `/nodes/{type}` | 获取特定节点信息 |
| POST | `/graph/create` | 创建图 |
| POST | `/graph/execute` | 执行图 |
| GET | `/graph/{id}/status` | 查询执行状态 |
| GET | `/graph/{id}/outputs` | 获取执行输出 |
| DELETE | `/graph/{id}` | 删除图 |
| GET | `/graphs` | 列出所有图 |
| GET | `/executions` | 列出所有执行 |
| POST | `/clear` | 清空所有数据 |

## 常见问题

### Q: 如何持久化图和执行结果？

A: 当前版本使用内存存储。要实现持久化，可以：
1. 修改 `api.py` 中的存储机制
2. 使用数据库（SQLite、PostgreSQL 等）
3. 使用文件系统存储 JSON

### Q: 如何处理大规模计算？

A: 这是一个最简实现。对于大规模计算，考虑：
1. 使用异步执行
2. 添加任务队列（Celery）
3. 分布式执行

### Q: 如何添加用户认证？

A: 在 `api.py` 中添加 FastAPI 的认证中间件：
```python
from fastapi.security import HTTPBearer
```

### Q: 执行失败怎么办？

A: 检查：
1. 节点输入是否正确
2. 图是否有循环依赖
3. 查看执行结果中的错误信息

## 下一步

1. 阅读 [README.md](README.md) 了解详细架构
2. 查看 [example_usage.py](example_usage.py) 学习更多用法
3. 运行 [test_example.py](test_example.py) 验证功能
4. 创建自己的节点类型
5. 集成到你的应用中

## 许可证

本项目是 RoboCute 项目的一部分。

