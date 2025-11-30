# Scene Context 传递问题修复

## 问题症状

执行 Node Graph 时报错：
```
RuntimeError: EntityInputNode requires a scene context
```

## 根本原因

在 `src/robocute/api.py` 的 `execute_graph` 函数中，从 `graph_definition` 创建图时**没有传递 scene_context**：

### 错误的代码（修复前）
```python
elif request.graph_definition:
    # 临时创建并执行图
    execution_id = f"exec_{uuid.uuid4()}"
    
    # ❌ 创建图时没有传递 scene_context
    graph = NodeGraph.from_definition(request.graph_definition, execution_id)
    
    # ... 验证图
    
# ❌ 在图创建之后才创建 scene_context
scene_context = SceneContext(_scene) if _scene else None
executor = GraphExecutor(graph, scene_context)
```

**问题**：
1. 图在创建时没有 scene_context
2. 节点在实例化时也就没有收到 context
3. 虽然后来 executor 有 context，但节点已经创建完成，无法追加 context

### 正确的代码（修复后）
```python
elif request.graph_definition:
    # 临时创建并执行图
    execution_id = f"exec_{uuid.uuid4()}"
    
    # ✅ 先创建 scene_context
    if _scene:
        print(f"[API] Creating scene context with {len(_scene.get_all_entities())} entities")
        scene_context = SceneContext(_scene)
    else:
        print("[API] WARNING: No scene available, graph will have no context")
        scene_context = None
    
    # ✅ 创建图时传递 scene_context
    graph = NodeGraph.from_definition(request.graph_definition, execution_id, scene_context)
    
    # ... 验证图

# ✅ 对于 graph_id 情况，创建新的 scene_context
if request.graph_id:
    scene_context = SceneContext(_scene) if _scene else None
else:
    # ✅ 对于 graph_definition 情况，使用图中已有的 context
    scene_context = graph.scene_context

executor = GraphExecutor(graph, scene_context)
```

## 数据流

### 错误的流程（修复前）
```
request.graph_definition
    ↓
NodeGraph.from_definition(definition, graph_id)
    ↓ scene_context = None ❌
NodeGraph.__init__(graph_id, scene_context=None)
    ↓
add_node() → registry.create_node(type, id, context=None)
    ↓
EntityInputNode(node_id, context=None) ❌
    ↓
node.execute() → self.context is None → RuntimeError ✗
```

### 正确的流程（修复后）
```
request.graph_definition
    ↓
scene_context = SceneContext(_scene) ✅
    ↓
NodeGraph.from_definition(definition, graph_id, scene_context)
    ↓
NodeGraph.__init__(graph_id, scene_context) ✅
    ↓
add_node() → registry.create_node(type, id, self.scene_context) ✅
    ↓
EntityInputNode(node_id, context) ✅
    ↓
node.execute() → self.context is not None → Success ✓
```

## 关键点

1. **Scene Context 必须在创建图之前准备好**
   - 因为节点在图创建过程中被实例化
   - 实例化时需要传入 context

2. **NodeGraph.from_definition() 的第三个参数是 scene_context**
   ```python
   def from_definition(cls, definition: GraphDefinition, 
                      graph_id: str = "default", 
                      scene_context: Optional['SceneContext'] = None)
   ```

3. **两种图创建方式需要不同处理**
   - `graph_id`: 从已存在的图执行，需要新建 scene_context
   - `graph_definition`: 临时创建图，在创建时传入 scene_context

## 验证

修复后的日志应该显示：
```
[API] Creating graph from definition: exec_xxx
[API] Creating scene context with 1 entities ✅
[NodeGraph] Creating graph 'exec_xxx' from definition
[NodeGraph] Scene context provided: True ✅
[NodeGraph] Adding 3 nodes...
[NodeGraph]   Creating node 'entity_input' of type 'entity_input'
[NodeGraph]   Scene context available: True ✅
[NodeRegistry] Creating instance of EntityInputNode
[NodeRegistry]   Context provided: True ✅
[NodeRegistry] ✓ Node instance created successfully
...
[Executor] Starting execution of graph 'exec_xxx'
[Executor] Scene context available: True ✅
[EntityInputNode] Looking for entity ID: 1
[EntityInputNode] Scene context available, 1 entities in scene ✅
[EntityInputNode] ✓ Found entity: Robot (ID: 1)
```

## 总结

**问题**：Scene context 在图创建后才准备，节点已经实例化完成

**修复**：在图创建前准备 scene context，作为参数传入 from_definition()

**结果**：节点在实例化时就能获得 scene context，执行时可以访问场景数据

这是一个经典的**依赖注入时序问题** - 依赖必须在对象创建前准备好。

