# 调试改进总结

## 问题
用户报告执行失败（execution failed），需要详细日志来诊断问题。

## 解决方案
在整个执行流程的关键点添加详细日志，从 Editor 发起请求到 Python 执行完成的每一步都可追踪。

## 修改的文件

### Python 服务器端

1. **`src/robocute/api.py`**
   - 请求接收日志（包括节点和连接数量）
   - 每个节点的定义详情
   - 图验证结果
   - Scene context 可用性
   - 执行结果摘要
   - 完整异常堆栈

2. **`src/robocute/graph.py`**
   - 图创建过程
   - 节点添加过程
   - 静态输入设置
   - 连接创建过程

3. **`src/robocute/node_registry.py`**
   - 节点类型查找
   - 注册类型列表
   - 节点实例化过程
   - 实例化异常捕获

4. **`src/robocute/executor.py`**
   - 执行开始/结束通知
   - 图验证过程
   - 拓扑排序结果
   - 每个节点的执行状态
   - 节点输入/输出详情
   - 节点执行异常

5. **`samples/animation_nodes.py`**
   - EntityInputNode: Entity 查找过程
   - RotationAnimationNode: 动画生成过程
   - AnimationOutputNode: 存储过程

### C++ Editor 端

6. **`rbc/editor/src/components/NodeEditor.cpp`**
   - 图序列化后的 JSON 预览
   - HTTP 请求状态
   - 服务器端执行状态和错误
   - 输出为空提示

## 调试工作流

### 步骤 1: 运行独立测试
```bash
python samples/test_execution_debug.py
```

这个脚本会：
- 创建场景和实体
- 检查节点注册状态
- 手动测试 EntityInputNode
- 构建并执行图
- 验证动画存储

**如果通过**：问题在 Editor → Server 通信
**如果失败**：问题在 Python 执行逻辑

### 步骤 2: 运行完整服务器
```bash
python main.py
```

观察启动日志：
```
Registered node: entity_input (Entity Input)
Registered node: rotation_animation (Rotation Animation)
Registered node: animation_output (Animation Output)
Created entity: Robot (ID: 1)
Editor Service started on port 5555
```

确认：
- ✅ 所有动画节点已注册
- ✅ 实体已创建
- ✅ 服务器在正确端口启动

### 步骤 3: 在 Editor 中执行图

查看服务器控制台，追踪完整流程：

#### 3.1 请求接收
```
[API] Graph execution request received
[API] Creating graph from definition: exec_xxx
[API] Nodes: 3
```

#### 3.2 节点创建
```
[NodeGraph] Adding 3 nodes...
[NodeGraph]   Adding node 'entity_input' (type: entity_input)
[NodeRegistry] Creating instance of EntityInputNode
[NodeRegistry]   Context provided: True
[NodeRegistry] ✓ Node instance created successfully
```

#### 3.3 图验证
```
[Executor] Validating graph...
[Executor] ✓ Validation passed
[Executor] ✓ Execution order: entity_input → rotation_anim → anim_output
```

#### 3.4 节点执行
```
[Executor] Executing node 'entity_input'
[EntityInputNode] Looking for entity ID: 1
[EntityInputNode] ✓ Found entity: Robot (ID: 1)
[Executor]   ✓ Node 'entity_input' completed
```

#### 3.5 结果
```
[Executor] ✓ All nodes executed successfully
[API] Execution finished: completed
```

### 步骤 4: 查看 Editor 执行面板

应显示：
```
=== Starting Graph Execution ===
Graph: 3 nodes, 2 connections
Graph Definition: {...}
Execution ID: exec_xxx
Status: completed
Node 'entity_input': {...}
Execution Completed
```

## 常见问题快速定位

### 问题 A: 节点未注册
**症状**: `[NodeRegistry] ✗ Node type 'xxx' not found`

**原因**: animation_nodes 未导入

**修复**: 
```python
# 在 main.py 中
import sys
sys.path.insert(0, 'samples')
import animation_nodes
```

### 问题 B: 无 Scene Context
**症状**: `[EntityInputNode] ✗ No scene context available!`

**原因**: 
1. `rbc.set_scene(scene)` 未调用
2. 或在 EditorService.start() 之后调用

**修复**:
```python
# 在 main.py，启动服务前
rbc.set_scene(scene)
editor_service.start(port=5555)
```

### 问题 C: Entity 不存在
**症状**: `[EntityInputNode] ✗ Entity 1 not found`

**原因**: 场景中没有对应 ID 的实体

**修复**:
```python
# 确保创建了实体
robot = scene.create_entity("Robot")
print(f"Created entity ID: {robot.id}")  # 使用这个 ID

# 在 NodeGraph 中使用正确的 entity_id
```

### 问题 D: 输入未连接
**症状**: `[RotationAnimationNode] ✗ No entity input provided`

**原因**: 节点连接错误或输出名称不匹配

**修复**: 检查连接定义
```python
rbc.NodeConnection(
    from_node="entity_input",
    from_output="entity",  # 必须匹配 EntityInputNode 的输出名称
    to_node="rotation_anim",
    to_input="entity"  # 必须匹配 RotationAnimationNode 的输入名称
)
```

## 日志解读技巧

1. **按顺序查找第一个 ✗**
   - 找到第一个失败点，后续都会连锁失败
   - 专注修复第一个错误

2. **检查 Context 传递链**
   - API → Executor: `Scene context available: True`
   - Executor → Node: `Context provided: True`
   - Node → 使用: `Scene context available, N entities`

3. **验证数据流**
   - 每个节点的输出应该出现在下一个节点的输入中
   - 检查 `[Executor] Outputs: {...}` 是否正确

4. **查看完整堆栈**
   - 所有异常都会打印完整堆栈
   - 从堆栈底部向上看，找到实际错误位置

## 快速测试命令

```bash
# 测试 1: 独立执行测试（无需 Editor）
python samples/test_execution_debug.py

# 测试 2: 完整服务器（需要 Editor）
python main.py

# 在另一个终端查看实时日志（Linux/Mac）
tail -f main.log

# Windows PowerShell
Get-Content main.log -Wait
```

## 日志输出示例

### 成功执行的完整日志
```
[API] Graph execution request received
[API] Creating graph from definition: exec_12345
[API] Nodes: 3
[API]   Node: entity_input (type: entity_input)
[API]     Inputs: {'entity_id': 1}
...
[Executor] Starting execution of graph 'exec_12345'
[Executor] Scene context available: True
...
[EntityInputNode] Looking for entity ID: 1
[EntityInputNode] ✓ Found entity: Robot (ID: 1)
...
[RotationAnimationNode] ✓ Generated 61 keyframes
...
[AnimationOutputNode] Stored animation 'test_rotation' with 60 frames
...
[Executor] ✓ All nodes executed successfully
[API] Execution finished: completed
```

### 失败执行的日志（示例）
```
[API] Graph execution request received
...
[EntityInputNode] Looking for entity ID: 1
[EntityInputNode] ✗ No scene context available!
[Executor]   ✗ Node 'entity_input' failed
RuntimeError: EntityInputNode requires a scene context
    at animation_nodes.py:45
[Executor] ✗ Stopping execution due to node 'entity_input' failure
[API] Execution finished: failed
[API]   Error: Node entity_input failed: EntityInputNode requires a scene context
```

从这个日志可以立即看出：问题在于 scene context 未传递给节点。

## 下一步

1. 运行 `test_execution_debug.py` 验证 Python 侧逻辑正确
2. 运行 `main.py` 并在 Editor 中执行图
3. 对比两次的日志输出，找出差异
4. 根据日志中的第一个 ✗ 标记定位问题

所有关键信息都会在日志中显示，无需猜测！

