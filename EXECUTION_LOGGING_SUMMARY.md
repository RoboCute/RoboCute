# 执行日志系统添加总结

## 目的
添加详细的日志系统来诊断 "execution failed" 问题，追踪从 Editor 发起请求到 Python 执行完成的完整流程。

## 添加的日志点

### Python 服务器端（共 5 层）

#### 1. API 层 - 请求接收和响应 (`src/robocute/api.py`)
- ✅ 请求接收通知（包含节点数量和连接数量）
- ✅ 每个节点定义的详细信息
- ✅ 图验证结果
- ✅ Scene context 可用性检查
- ✅ 执行结果摘要（每个节点的状态）
- ✅ 异常完整堆栈信息

#### 2. NodeGraph - 图构建 (`src/robocute/graph.py`)
- ✅ 图创建通知（包含 scene context 状态）
- ✅ 每个节点添加过程
- ✅ 节点实例创建确认
- ✅ 静态输入值设置
- ✅ 连接创建过程
- ✅ 图构建完成确认

#### 3. NodeRegistry - 节点实例化 (`src/robocute/node_registry.py`)
- ✅ 节点类型查找结果
- ✅ 注册类型列表（查找失败时）
- ✅ 节点实例化过程
- ✅ Context 传递确认
- ✅ 实例化异常捕获和堆栈

#### 4. GraphExecutor - 执行引擎 (`src/robocute/executor.py`)
- ✅ 执行开始通知
- ✅ Scene context 可用性
- ✅ 图验证过程
- ✅ 拓扑排序结果（执行顺序）
- ✅ 每个节点执行前后状态
- ✅ 节点输入和输出详情
- ✅ 节点异常堆栈
- ✅ 执行完成摘要

#### 5. Animation Nodes - 业务逻辑 (`samples/animation_nodes.py`)

**EntityInputNode**:
- ✅ Entity ID 查找
- ✅ Scene context 可用性检查
- ✅ 场景中实体数量
- ✅ Entity 查找结果
- ✅ 可用实体列表（查找失败时）

**RotationAnimationNode**:
- ✅ Entity 输入接收
- ✅ 动画生成开始
- ✅ 生成的关键帧数量
- ✅ 总帧数

**AnimationOutputNode**:
- ✅ Animation 输入类型
- ✅ 存储操作
- ✅ Scene context 检查
- ✅ 存储成功确认

### C++ Editor 端（2 个组件）

#### 1. NodeEditor - 图序列化和结果显示 (`rbc/editor/src/components/NodeEditor.cpp`)
- ✅ 执行开始标记
- ✅ 图定义的 JSON（前 500 字符）
- ✅ 执行 ID
- ✅ HTTP 请求失败提示
- ✅ 服务器端执行状态
- ✅ 服务器端错误信息
- ✅ 输出为空提示
- ✅ 执行完成状态

#### 2. EditorScene - 场景同步 (已有日志)
- ✅ 场景初始化
- ✅ 实体同步
- ✅ 灯光延迟初始化

## 日志流程图

```
Editor: 用户点击 Execute
    ↓
[NodeEditor] Serialize graph → JSON
    ↓
[HttpClient] POST /graph/execute
    ↓
──────────────────────────────────
    ↓
[API] Request received
    ↓
[API] Parse graph definition
    ↓
[NodeGraph] Create graph
    ├─→ [NodeRegistry] Create nodes
    │   └─→ [Nodes] __init__ with context
    └─→ [NodeGraph] Add connections
    ↓
[API] Create executor with scene context
    ↓
[Executor] Validate graph
    ↓
[Executor] Topological sort
    ↓
[Executor] Execute nodes in order
    ├─→ [Node 1] execute()
    ├─→ [Node 2] execute()
    └─→ [Node 3] execute()
    ↓
[Executor] Return results
    ↓
[API] Store results & respond
    ↓
──────────────────────────────────
    ↓
[HttpClient] Response received
    ↓
[NodeEditor] Fetch outputs
    ↓
[HttpClient] GET /graph/{id}/outputs
    ↓
[NodeEditor] Display results
```

## 使用方式

### 运行并查看日志
```bash
python main.py
```

### 执行图并诊断

1. 在 Editor 中创建节点图
2. 点击 Execute
3. 查看服务器控制台，按照日志顺序检查：
   - 每个 ✅ 表示该步骤成功
   - 每个 ✗ 表示该步骤失败，立即定位问题
   - 完整的堆栈信息帮助调试

### 常见错误模式

**模式 1: 节点未注册**
```
[NodeRegistry] ✗ Node type 'entity_input' not found
→ 检查 import animation_nodes
```

**模式 2: 无 Scene Context**
```
[API] WARNING: No scene available, executing without context
[EntityInputNode] ✗ No scene context available!
→ 检查 rbc.set_scene(scene)
```

**模式 3: Entity 不存在**
```
[EntityInputNode] ✗ Entity 1 not found
[EntityInputNode] Available entities: []
→ 检查场景中是否创建了实体
```

**模式 4: 输入未连接**
```
[RotationAnimationNode] ✗ No entity input provided
→ 检查节点连接是否正确
```

## 日志级别

所有日志使用 `print()` 输出，确保在所有环境下都能看到。关键标记：
- `✓` - 成功
- `✗` - 失败
- `WARNING` - 警告
- `Error` - 错误

## 清理

如果需要减少日志输出，可以：
1. 将 `print()` 改为条件输出（基于调试标志）
2. 使用 logging 模块
3. 移除不需要的日志点

但在 MVP 阶段，建议保留所有日志以便快速诊断问题。

