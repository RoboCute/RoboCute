# 调试日志指南

## 已添加的日志系统

为了诊断 "execution failed" 问题，已在关键位置添加详细日志。

## 日志位置

### Python 服务器端

#### 1. API 层 (`src/robocute/api.py`)
```
[API] Graph execution request received
[API] Creating graph from definition: exec_xxx
[API] Nodes: N
[API] Connections: N
[API]   Node: node_id (type: node_type)
[API]     Inputs: {...}
[API] Validating graph...
[API] ✓ Graph validation passed / ✗ Graph validation failed
[API] Using scene context with N entities
[API] Creating executor...
[API] Starting graph execution...
[API] Execution finished: completed/failed
[API]   Error: error message
[API]   ✓/✗ Node 'node_id': status
[API] ValueError/Exception: detailed error with traceback
```

#### 2. Graph 层 (`src/robocute/graph.py`)
```
[NodeGraph] Creating graph 'graph_id' from definition
[NodeGraph] Scene context provided: True/False
[NodeGraph] Adding N nodes...
[NodeGraph]   Adding node 'node_id' (type: node_type)
[NodeGraph]   ✓ Node 'node_id' created / ✗ Failed to create node
[NodeGraph]   Setting inputs: {...}
[NodeGraph] Adding N connections...
[NodeGraph]   from_node.output → to_node.input
[NodeGraph]   ✗ Failed to create connection
[NodeGraph] ✓ Graph created successfully
```

#### 3. NodeRegistry 层 (`src/robocute/node_registry.py`)
```
[NodeRegistry] ✗ Node type 'xxx' not found in registry
[NodeRegistry]   Registered types: [list of types]
[NodeRegistry] Creating instance of NodeClassName
[NodeRegistry]   Context provided: True/False
[NodeRegistry] ✓ Node instance created successfully
[NodeRegistry] ✗ Failed to instantiate node: error
```

#### 4. Executor 层 (`src/robocute/executor.py`)
```
[Executor] Starting execution of graph 'graph_id'
[Executor] Scene context available: True/False
[Executor] Validating graph...
[Executor] ✓ Validation passed / ✗ Validation failed
[Executor] Computing execution order...
[Executor] ✓ Execution order: node1 → node2 → node3
[Executor] Executing node 'node_id' (type: NodeClass)
[Executor]   Inputs: {...}
[Executor]   ✓ Node 'node_id' completed / ✗ Node 'node_id' failed
[Executor]   Outputs: {...}
[Executor] ✗ Stopping execution due to node 'xxx' failure
[Executor] ✓ All nodes executed successfully
```

#### 5. Animation Nodes (`samples/animation_nodes.py`)
```
[EntityInputNode] Looking for entity ID: N
[EntityInputNode] ✗ No scene context available!
[EntityInputNode] Scene context available, N entities in scene
[EntityInputNode] ✗ Entity N not found
[EntityInputNode]   Available entities: [1, 2, 3]
[EntityInputNode] ✓ Found entity: name (ID: N)

[RotationAnimationNode] Entity input: {...}
[RotationAnimationNode] ✗ No entity input provided
[RotationAnimationNode] Generating rotation animation for entity N
[RotationAnimationNode] ✓ Generated N keyframes
[RotationAnimationNode]   Total frames: N

[AnimationOutputNode] Animation input received: <class 'AnimationSequence'>
[AnimationOutputNode] ✗ No animation input provided
[AnimationOutputNode] Storing animation as 'name'
[AnimationOutputNode] ✗ No scene context available!
[AnimationOutputNode] Stored animation 'name' with N frames
```

### C++ Editor 端

#### 1. NodeEditor 执行面板
```
=== Starting Graph Execution ===
Graph: N nodes, N connections
Graph Definition:
{... JSON ...}
Execution ID: exec_xxx
Fetching execution outputs...
Status: completed/failed
Server error: error message
Node 'node_id': {...}
Execution Completed
```

#### 2. EditorScene 同步
```
EditorScene initialized (light will be initialized on first entity)
EditorScene: Syncing N entities, N resources
  Resource N: path
  Entity N: name, has_render=true/false
  Updating entity N transform
  Adding new entity N with mesh path
Adding entity N with mesh: path
EditorScene: Light initialized
Entity N added to TLAS at index N
```

## 常见问题诊断

### 问题 1: No scene context available

**症状**：
```
[EntityInputNode] ✗ No scene context available!
```

**原因**：
- `_scene` 全局变量未设置
- API 中 `set_scene(scene)` 未调用

**检查**：
```python
# 在 main.py 中
rbc.set_scene(scene)  # 必须在启动 EditorService 前调用
```

### 问题 2: Node type not found

**症状**：
```
[NodeRegistry] ✗ Node type 'entity_input' not found in registry
[NodeRegistry]   Registered types: [...]
```

**原因**：
- animation_nodes 未导入
- 节点注册失败

**检查**：
```python
# 在 main.py 或 node_server.py
import animation_nodes  # 确保导入
```

### 问题 3: Entity not found

**症状**：
```
[EntityInputNode] ✗ Entity 1 not found
[EntityInputNode] Available entities: []
```

**原因**：
- 场景中没有实体
- Entity ID 不匹配

**检查**：
```python
# 在 main.py 中创建实体
robot = scene.create_entity("Robot")
print(f"Created entity ID: {robot.id}")  # 记住这个 ID
```

### 问题 4: Graph validation failed

**症状**：
```
[API] ✗ Graph validation failed: error message
```

**原因**：
- 连接的端口不存在
- 节点 ID 重复
- 循环依赖

**检查**：
- 确认节点类型的输入输出定义
- 检查连接是否正确

## 调试步骤

### 1. 启动服务器并查看日志
```bash
python main.py
```

应该看到：
```
Registered node: entity_input (Entity Input)
Registered node: rotation_animation (Rotation Animation)
Registered node: animation_output (Animation Output)
Created entity: Robot (ID: 1)
Editor Service started on port 5555
```

### 2. 在 Editor 中执行 Graph

观察服务器控制台输出，按照日志流程诊断：

**a. Graph 接收**
```
[API] Graph execution request received
[API] Creating graph from definition: exec_xxx
[API] Nodes: 3
```
❌ 如果看不到这个，说明 HTTP 请求未到达

**b. Graph 创建**
```
[NodeGraph] Creating graph 'exec_xxx' from definition
[NodeGraph] Scene context provided: True
[NodeGraph] Adding 3 nodes...
```
❌ 如果 scene_context 是 False，检查 `rbc.set_scene(scene)` 是否调用

**c. 节点创建**
```
[NodeGraph]   Adding node 'entity_input' (type: entity_input)
[NodeRegistry] Creating instance of EntityInputNode
[NodeRegistry]   Context provided: True
[NodeRegistry] ✓ Node instance created successfully
```
❌ 如果节点创建失败，检查节点是否注册

**d. 节点执行**
```
[Executor] Starting execution of graph 'exec_xxx'
[Executor] Scene context available: True
[Executor] ✓ Execution order: entity_input → rotation_animation → animation_output
[Executor] Executing node 'entity_input'
[EntityInputNode] Looking for entity ID: 1
[EntityInputNode] Scene context available, 1 entities in scene
[EntityInputNode] ✓ Found entity: Robot (ID: 1)
```
❌ 如果找不到实体，检查 entity_id 是否正确

**e. 动画生成**
```
[RotationAnimationNode] Generating rotation animation for entity 1
[RotationAnimationNode] ✓ Generated 121 keyframes
```

**f. 动画存储**
```
[AnimationOutputNode] Storing animation as 'animation_name'
[AnimationOutputNode] Stored animation 'animation_name' with 120 frames
```

**g. 执行完成**
```
[Executor] ✓ All nodes executed successfully
[API] Execution finished: completed
```

### 3. 查看 Editor 执行面板

应该显示：
```
=== Starting Graph Execution ===
Graph: 3 nodes, 2 connections
Execution ID: exec_xxx
Fetching execution outputs...
Status: completed
Node 'entity_input': {...}
Node 'rotation_animation': {...}
Node 'animation_output': {...}
Execution Completed
```

## 快速诊断命令

### 检查节点注册
在 Python 控制台：
```python
import robocute as rbc
registry = rbc.get_registry()
print(registry.get_all_node_types())
# 应该包含: entity_input, rotation_animation, animation_output
```

### 检查场景状态
```python
print(f"Scene entities: {len(scene.get_all_entities())}")
for entity in scene.get_all_entities():
    print(f"  Entity {entity.id}: {entity.name}")
```

### 手动测试节点
```python
from robocute.scene_context import SceneContext
context = SceneContext(scene)
node = EntityInputNode("test", context)
node.set_input("entity_id", 1)
result = node.execute()
print(result)
```

## 预期完整日志流程

正常执行应该看到以下完整流程（按顺序）：

```
[API] Graph execution request received
[API] Creating graph from definition: exec_xxx
[API] Nodes: 3
[API]   Node: entity_input (type: entity_input)
[API]     Inputs: {'entity_id': 1}
[API]   Node: rotation_anim (type: rotation_animation)
[API]     Inputs: {...}
[API]   Node: anim_output (type: animation_output)
[API]     Inputs: {'name': 'test_rotation', 'fps': 30.0}
[API] Validating graph...
[API] ✓ Graph validation passed
[API] Using scene context with 1 entities
[API] Creating executor...
[API] Starting graph execution...

[Executor] Starting execution of graph 'exec_xxx'
[Executor] Scene context available: True
[Executor] Validating graph...
[Executor] ✓ Validation passed
[Executor] Computing execution order...
[Executor] ✓ Execution order: entity_input → rotation_anim → anim_output

[Executor] Executing node 'entity_input' (type: EntityInputNode)
[Executor]   Inputs: {'entity_id': 1}
[EntityInputNode] Looking for entity ID: 1
[EntityInputNode] Scene context available, 1 entities in scene
[EntityInputNode] ✓ Found entity: Robot (ID: 1)
[Executor]   ✓ Node 'entity_input' completed
[Executor]   Outputs: {'entity': {'id': 1, 'name': 'Robot'}}

[Executor] Executing node 'rotation_anim' (type: RotationAnimationNode)
[Executor]   Inputs: {'entity': {'id': 1, 'name': 'Robot'}, ...}
[RotationAnimationNode] Entity input: {'id': 1, 'name': 'Robot'}
[RotationAnimationNode] Generating rotation animation for entity 1
[RotationAnimationNode] ✓ Generated 121 keyframes
[RotationAnimationNode]   Total frames: 120
[Executor]   ✓ Node 'rotation_anim' completed

[Executor] Executing node 'anim_output' (type: AnimationOutputNode)
[AnimationOutputNode] Animation input received: <class 'AnimationSequence'>
[AnimationOutputNode] Storing animation as 'test_rotation'
[AnimationOutputNode] Stored animation 'test_rotation' with 120 frames
[Executor]   ✓ Node 'anim_output' completed

[Executor] ✓ All nodes executed successfully
[API] Execution finished: completed
[API]   ✓ Node 'entity_input': completed
[API]   ✓ Node 'rotation_anim': completed
[API]   ✓ Node 'anim_output': completed
```

## 下一步

运行服务器并执行图，查看控制台输出，找到第一个 ✗ 错误标记，即可定位问题所在。

