# Editor V0.1 最小可用实例

- start date: 
- finish date: 2025-11-30

这一次的开发属于v0.1


## 核心理念：Python-First + 可选编辑器

### 关键设计原则

1. **Python是唯一真实来源（Single Source of Truth）**
   - 场景、资源、状态的权威数据都在Python Server端
   - 编辑器是可选的，Python代码可以完全脱离编辑器运行
   - 支持无头模式（Headless）进行离线渲染和大规模仿真

2. **编辑器是调试器/可视化工具**
   - 编辑器不拥有场景数据，仅缓存Server端的镜像副本用于显示
   - 编辑器的任何"编辑"操作最终都是向Server发送命令，会和链接的Server定期同步场景，同时也会保证在开始执行命令的时候确保同步
   - 编辑器主要用于提高开发体验，而非必需组件

3. **命令模式交互**
   - Editor → Server：缓存编辑命令（Create Entity, Set Transform, etc.）并定期同步
   - Server → Editor：推送同步结果

4. **代码优先，GUI辅助**
   - 所有功能都可以通过Python代码完成
   - 编辑器提供GUI来简化繁琐的参数设置和可视化调试
   - 编辑器的价值主要体现在
     - 使用Viewport选中指定物体并使用Detail面板调整参数
     - 使用NodeGraph编辑策略节点
     - 对结果丰富的可视化功能

---

## 系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     Python Server (Core)                    │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Scene Manager (Ground Truth)             │  │
│  │  - Entity Management (ECS)                            │  │
│  │  - Resource Management                                │  │
│  │  - Physics Simulation                                 │  │
│  │  - Animation System                                   │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Node Execution Engine                    │  │
│  │  - Node Registry                                      │  │
│  │  - Runtime Graph Execution                            │  │
│  │  - Algorithm Implementation                           │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Editor Service (Optional)                │  │
│  │  - Command Handler (接收Editor命令)                    │  │
│  │  - State Broadcaster (推送场景状态)                    │  │
│  │  - IPC Server (ZeroMQ/WebSocket)                      │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            ↕ IPC (Optional)
┌─────────────────────────────────────────────────────────────┐
│              C++ Editor (Optional Visualizer)               │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Scene View (Read-Only Mirror)            │  │
│  │  - Entity Cache (镜像Server状态)                       │  │
│  │  - Resource Cache                                     │  │
│  │  - Incremental Update Handler                         │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              UI Components                            │  │
│  │  - 3D Viewport (可视化)                                │  │
│  │  - Node Graph Editor (节点图GUI)                       │  │
│  │  - Property Inspector (参数调整GUI)                    │  │
│  │  - Timeline (动画回放)                                 │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Command Dispatcher                       │  │
│  │  - 将UI操作转换为Server命令                            │  │
│  │  - 等待Server确认后更新本地视图                         │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Python Server 端设计

### 1. 场景管理系统（权威数据源）

```python
# Python端场景管理器
class RBCScene:
    """场景的唯一权威数据源"""
    
    def __init__(self):
        self.entity_manager = EntityManager()
        self.resource_manager = ResourceManager()
        self.systems = SystemScheduler()
        
    # === 场景操作 API ===
    def create_entity(self, name: str = "") -> EntityID:
        """创建实体"""
        entity_id = self.entity_manager.create()
        if name:
            self.entity_manager.add_component(entity_id, NameComponent(name))
        self._notify_editor_entity_created(entity_id)
        return entity_id
    
    def destroy_entity(self, entity_id: EntityID):
        """销毁实体"""
        self.entity_manager.destroy(entity_id)
        self._notify_editor_entity_destroyed(entity_id)
    
    def add_component(self, entity_id: EntityID, component: Component):
        """添加组件"""
        self.entity_manager.add_component(entity_id, component)
        self._notify_editor_component_changed(entity_id, component)
    
    # === 场景文件操作 ===
    def load_scene(self, path: str) -> bool:
        """从文件加载场景（支持.rbc/.usd/.json）"""
        scene_data = self._load_from_file(path)
        self._apply_scene_data(scene_data)
        self._notify_editor_full_sync()  # 通知Editor完整同步
        return True
    
    def save_scene(self, path: str) -> bool:
        """保存场景到文件"""
        scene_data = self._serialize_scene()
        return self._save_to_file(path, scene_data)
    
    # === 仿真控制 ===
    def update(self, delta_time: float):
        """更新仿真（可在无Editor情况下运行）"""
        self.systems.update_all(delta_time)
        
        # 如果Editor连接，推送高频数据
        if self.editor_service and self.editor_service.is_connected():
            self._push_simulation_state()
    
    # === Editor通知（仅当Editor连接时）===
    def _notify_editor_entity_created(self, entity_id: EntityID):
        if self.editor_service:
            self.editor_service.broadcast_entity_created(entity_id)
    
    def _notify_editor_component_changed(self, entity_id: EntityID, component: Component):
        if self.editor_service:
            self.editor_service.broadcast_component_update(entity_id, component)
    
    def _push_simulation_state(self):
        """推送仿真状态到Editor（动画、物理等高频数据）"""
        updates = self._collect_simulation_updates()
        self.editor_service.broadcast_simulation_state(updates)
```

### 2. Editor Service（可选模块）

Python Server可选择性启动Editor Service来支持编辑器连接：

```python
class EditorService:
    """编辑器服务（可选，用于支持Editor连接）"""
    
    def __init__(self, scene: RBCScene):
        self.scene = scene
        self.ipc_server = None  # ZeroMQ/WebSocket
        self.connected_editors = []
        
    def start(self, port: int = 5555):
        """启动Editor服务"""
        self.ipc_server = IPCServer(port)
        self.ipc_server.on_message(self._handle_editor_command)
        self.ipc_server.start()
        print(f"[EditorService] Listening on port {port}")
    
    def stop(self):
        """停止Editor服务"""
        if self.ipc_server:
            self.ipc_server.stop()
    
    def is_connected(self) -> bool:
        """是否有Editor连接"""
        return len(self.connected_editors) > 0
    
    # === 命令处理 ===
    def _handle_editor_command(self, command: dict):
        """处理Editor发来的命令"""
        cmd_type = command["type"]
        
        if cmd_type == "create_entity":
            entity_id = self.scene.create_entity(command["name"])
            return {"success": True, "entity_id": entity_id}
        
        elif cmd_type == "set_transform":
            entity_id = command["entity_id"]
            position = command["position"]
            rotation = command["rotation"]
            
            transform = self.scene.entity_manager.get_component(entity_id, TransformComponent)
            if transform:
                transform.position = position
                transform.rotation = rotation
                self.scene.entity_manager.mark_dirty(entity_id)
                self.broadcast_component_update(entity_id, transform)
                return {"success": True}
            return {"success": False, "error": "Entity not found"}
        
        elif cmd_type == "execute_graph":
            graph_data = command["graph"]
            success = self.scene.execute_node_graph(graph_data)
            return {"success": success}
        
        # ... 处理其他命令
    
    # === 状态广播 ===
    def broadcast_entity_created(self, entity_id: EntityID):
        """广播实体创建事件"""
        message = {
            "event": "entity_created",
            "entity_id": entity_id,
            "components": self._serialize_entity_components(entity_id)
        }
        self._broadcast(message)
    
    def broadcast_component_update(self, entity_id: EntityID, component: Component):
        """广播组件更新"""
        message = {
            "event": "component_updated",
            "entity_id": entity_id,
            "component": self._serialize_component(component)
        }
        self._broadcast(message)
    
    def broadcast_simulation_state(self, updates: list):
        """广播仿真状态（高频数据）"""
        message = {
            "event": "simulation_update",
            "updates": updates
        }
        self._broadcast(message)
    
    def broadcast_full_scene(self):
        """广播完整场景（Editor连接时的初始同步）"""
        message = {
            "event": "full_scene",
            "entities": self._serialize_all_entities(),
            "resources": self._serialize_resources()
        }
        self._broadcast(message)
    
    def _broadcast(self, message: dict):
        """向所有连接的Editor广播消息"""
        for editor in self.connected_editors:
            editor.send(message)
```
## C++ Editor 端设计

### 1. 场景视图（只读镜像）

Editor维护Server场景的本地缓存，但**不拥有数据**：

```cpp
// 编辑器场景视图（镜像Server状态）
class EditorSceneView {
public:
    // === 场景查询（只读）===
    std::vector<EntityID> get_all_entities() const;
    const EntityMetadata* get_entity_metadata(EntityID entity) const;
    
    template<typename T>
    const T* get_component(EntityID entity) const;  // 注意：返回const指针
    
    // === 接收Server更新 ===
    void on_entity_created(EntityID entity, const std::vector<uint8_t>& data);
    void on_entity_destroyed(EntityID entity);
    void on_component_updated(EntityID entity, ComponentTypeID type, const std::vector<uint8_t>& data);
    void on_full_scene_sync(const std::vector<uint8_t>& scene_data);
    void on_simulation_update(const std::vector<SimulationUpdate>& updates);
    
    // === 选择管理（Editor本地状态）===
    void select_entity(EntityID entity);
    const std::set<EntityID>& get_selected_entities() const;
    
private:
    // 场景数据缓存（只读镜像）
    std::unordered_map<EntityID, EntityData> entities_;
    std::unordered_map<ResourceID, ResourceMetadata> resources_;
    
    // Editor本地状态
    std::set<EntityID> selected_entities_;
    bool scene_loaded_ = false;
    
    // 增量更新管理
    void apply_incremental_update(const Update& update);
};
```

### 2. 命令分发器

Editor的所有"编辑"操作都转换为命令发送到Server：

```cpp
class EditorCommandDispatcher {
public:
    EditorCommandDispatcher(ServerConnection& connection);
    
    // === 实体操作命令 ===
    std::future<EntityID> create_entity(const std::string& name = "");
    std::future<bool> destroy_entity(EntityID entity);
    std::future<bool> duplicate_entity(EntityID entity);
    
    // === 组件操作命令 ===
    template<typename T>
    std::future<bool> add_component(EntityID entity, const T& component);
    
    template<typename T>
    std::future<bool> update_component(EntityID entity, const T& component);
    
    template<typename T>
    std::future<bool> remove_component(EntityID entity);
    
    // === 变换操作命令 ===
    std::future<bool> set_transform(EntityID entity, const Vector3& pos, const Quaternion& rot);
    std::future<bool> translate(EntityID entity, const Vector3& delta);
    std::future<bool> rotate(EntityID entity, const Quaternion& delta);
    
    // === 场景操作命令 ===
    std::future<bool> request_scene_load(const std::string& path);
    std::future<bool> request_scene_save(const std::string& path);
    
    // === 节点图操作命令 ===
    std::future<bool> execute_node_graph(const std::string& graph_json);
    std::future<bool> stop_execution();
    
    // === 命令模式实现 ===
    struct Command {
        std::string type;
        nlohmann::json params;
        std::promise<nlohmann::json> result_promise;
        
        std::chrono::steady_clock::time_point timestamp;
    };
    
    void send_command(const Command& cmd);
    void handle_command_result(uint64_t command_id, const nlohmann::json& result);
    
private:
    ServerConnection& connection_;
    std::unordered_map<uint64_t, Command> pending_commands_;
    std::atomic<uint64_t> next_command_id_{1};
    std::mutex commands_mutex_;
};
```

### 3. Server连接管理

```cpp
class ServerConnection {
public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting,
        Error
    };
    
    // === 连接管理 ===
    bool connect(const std::string& address = "127.0.0.1", uint16_t port = 5555);
    void disconnect();
    ConnectionState get_state() const { return state_; }
    
    // === 初始同步 ===
    std::future<bool> request_full_scene();
    std::future<NodeManifest> request_node_manifest();
    
    // === 命令发送 ===
    std::future<nlohmann::json> send_command(const nlohmann::json& command);
    
    // === 事件订阅 ===
    using EntityEventCallback = std::function<void(EntityID, const std::vector<uint8_t>&)>;
    using SimulationUpdateCallback = std::function<void(const std::vector<SimulationUpdate>&)>;
    
    void on_entity_created(EntityEventCallback callback);
    void on_entity_destroyed(EntityEventCallback callback);
    void on_component_updated(EntityEventCallback callback);
    void on_simulation_update(SimulationUpdateCallback callback);
    void on_full_scene(EntityEventCallback callback);
    
    // === 高频数据流订阅 ===
    void subscribe_simulation_stream(bool enable);
    void set_stream_frequency(float hz);  // 设置期望的更新频率
    
private:
    std::unique_ptr<IPCClient> ipc_client_;  // ZeroMQ/WebSocket
    ConnectionState state_ = ConnectionState::Disconnected;
    
    // 消息处理线程
    std::thread message_thread_;
    std::atomic<bool> running_{false};
    
    void message_loop();
    void handle_message(const nlohmann::json& message);
    void dispatch_event(const std::string& event_type, const nlohmann::json& data);
    
    // 回调管理
    std::vector<EntityEventCallback> entity_created_callbacks_;
    std::vector<EntityEventCallback> entity_destroyed_callbacks_;
    std::vector<EntityEventCallback> component_updated_callbacks_;
    std::vector<SimulationUpdateCallback> simulation_update_callbacks_;
};
```

### 4. UI组件设计

#### 4.1 3D视口（纯可视化）

```cpp
class SceneViewport : public QOpenGLWidget {
    Q_OBJECT
    
public:
    explicit SceneViewport(EditorSceneView& scene_view, 
                          EditorCommandDispatcher& commands,
                          QWidget* parent = nullptr);
    
    // === 渲染 ===
    void render_scene();
    void render_gizmos();
    
    // === 交互（通过命令模式）===
    void on_gizmo_drag(EntityID entity, const Transform3D& new_transform);
    // 实现：
    // 1. 立即在本地预览新变换（乐观更新）
    // 2. 发送命令到Server
    // 3. 等待Server确认后正式更新本地视图
    
    // === 拾取 ===
    EntityID pick_entity(const QPoint& screen_pos);
    
signals:
    void entity_selected(EntityID entity);
    void transform_edit_started(EntityID entity);
    void transform_edit_finished(EntityID entity);
    
private:
    EditorSceneView& scene_view_;
    EditorCommandDispatcher& commands_;
    
    // 渲染器
    std::unique_ptr<EditorRenderer> renderer_;
    
    // Gizmo
    std::unique_ptr<TransformGizmo> gizmo_;
    
    // 乐观更新缓存
    struct OptimisticUpdate {
        EntityID entity_id;
        Transform3D original_transform;
        Transform3D preview_transform;
        std::future<bool> command_future;
    };
    std::optional<OptimisticUpdate> pending_transform_update_;
    
    void apply_optimistic_update(EntityID entity, const Transform3D& transform);
    void revert_optimistic_update();
};
```

#### 4.2 属性编辑器（命令模式）

```cpp
class PropertyInspectorWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit PropertyInspectorWidget(EditorSceneView& scene_view,
                                    EditorCommandDispatcher& commands,
                                    QWidget* parent = nullptr);
    
    void set_selected_entity(EntityID entity);
    
private slots:
    void on_property_changed(const QString& property_name, const QVariant& value);
    // 实现：
    // 1. 显示等待状态（Loading...）
    // 2. 发送update_component命令到Server
    // 3. 等待Server确认
    // 4. Server广播更新后，刷新UI显示
    
private:
    EditorSceneView& scene_view_;
    EditorCommandDispatcher& commands_;
    EntityID current_entity_ = INVALID_ENTITY;
    
    std::unordered_map<std::string, std::future<bool>> pending_updates_;
    
    void create_property_widgets();
    void refresh_from_scene_view();
};
```

#### 4.3 节点图编辑器（主要价值点）

节点图编辑是Editor的核心功能，因为纯代码创建复杂节点图比较繁琐：

```cpp
class NodeGraphEditor : public QGraphicsView {
    Q_OBJECT
    
public:
    explicit NodeGraphEditor(ServerConnection& connection,
                            EditorCommandDispatcher& commands,
                            QWidget* parent = nullptr);
    
    // === 节点操作 ===
    void add_node(const std::string& node_type, const QPointF& position);
    void remove_node(const std::string& node_id);
    void connect_nodes(const std::string& from_node, const std::string& from_socket,
                      const std::string& to_node, const std::string& to_socket);
    
    // === 图操作 ===
    bool load_graph(const std::string& path);  // 从文件加载
    bool save_graph(const std::string& path);  // 保存到文件（供Python加载）
    
    // === 执行（发送到Server）===
    void execute_graph();
    void stop_execution();
    
    // === 从Server同步节点定义 ===
    void refresh_node_manifest();
    
signals:
    void graph_modified();
    void execution_started();
    void execution_finished(bool success);
    
private:
    ServerConnection& connection_;
    EditorCommandDispatcher& commands_;
    
    // 节点定义（从Server获取）
    NodeManifest node_manifest_;
    
    // 当前编辑的图
    struct NodeGraph {
        std::vector<NodeInstance> nodes;
        std::vector<Connection> connections;
    };
    NodeGraph current_graph_;
    
    std::string serialize_graph_to_json() const;
    bool deserialize_graph_from_json(const std::string& json);
};
```

### 5. 资源管理（缓存模式）

Editor的资源管理器仅用于缓存和加载可视化所需的资源：

```cpp
class EditorResourceCache {
public:
    // === 资源请求（按需加载）===
    std::shared_ptr<Mesh> get_mesh(ResourceID id);
    std::shared_ptr<Texture> get_texture(ResourceID id);
    std::shared_ptr<Material> get_material(ResourceID id);
    
    // === 资源路径映射（从Server同步）===
    void register_resource_path(ResourceID id, const std::string& path);
    
    // === 缓存管理 ===
    void set_cache_budget(size_t bytes);
    void clear_unused();
    
    // === 异步加载 ===
    std::future<std::shared_ptr<Resource>> load_async(ResourceID id);
    
private:
    // 资源缓存（LRU）
    std::unordered_map<ResourceID, std::shared_ptr<Resource>> cache_;
    std::unordered_map<ResourceID, std::string> resource_paths_;
    
    // 加载队列
    AsyncResourceLoader loader_;
    
    // 注意：Editor不管理资源的创建和删除，只负责缓存
};
```

---

## 完整工作流程

### 1. 启动流程

```
┌─────────────────────────────────────────────────────────────┐
│ 方式 A: 带编辑器启动                                          │
├─────────────────────────────────────────────────────────────┤
│ 1. python main.py --with-editor                             │
│    └─> 初始化Scene                                           │
│    └─> 启动EditorService (port 5555)                        │
│    └─> subprocess.Popen("RBCEditor.exe")                    │
│                                                              │
│ 2. RBCEditor启动                                             │
│    └─> 连接到localhost:5555                                 │
│    └─> request_full_scene()                                 │
│    └─> request_node_manifest()                              │
│    └─> 显示UI                                                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ 方式 B: 无头模式（离线渲染/大规模仿真）                        │
├─────────────────────────────────────────────────────────────┤
│ 1. python headless_simulation.py                            │
│    └─> 初始化Scene                                           │
│    └─> 不启动EditorService                                   │
│    └─> 执行仿真循环                                           │
│    └─> 导出结果                                               │
│                                                              │
│ （完全不需要Editor）                                          │
└─────────────────────────────────────────────────────────────┘
```

### 2. 场景编辑流程（命令模式）

```
用户操作 (Editor UI)          Editor                Server (Ground Truth)
     |                         |                           |
  [点击Create Entity]          |                           |
     |-------------------->  拼装命令                        |
     |                    {type: "create_entity",           |
     |                     name: "NewRobot"}               |
     |                         |--send_command()---------->|
     |                         |                      执行创建
     |                         |                      entity_id=42
     |                         |<------返回结果------------|
     |                    收到 {success: true,             |
     |                          entity_id: 42}            |
     |                    等待场景更新...                   |
     |                         |                           |
     |                         |<---broadcast_entity_created(42)--|
     |                    更新本地视图                      |
     |<-----刷新UI---------scene_view.on_entity_created()  |
     |                         |                           |
  [显示新实体]                   |                           |
```

### 3. 变换编辑流程（乐观更新）

```
用户操作 (Editor)              Editor                    Server
     |                          |                           |
  [拖动Gizmo]                    |                           |
     |                    立即预览新位置                       |
     |                   (乐观更新，本地渲染)                   |
     |<----preview-----  apply_optimistic_update()          |
     |                          |                           |
     |                    同时发送命令                         |
     |                          |--set_transform()-------->|
     |                          |                      应用变换
  [持续拖动]                      |                      mark_dirty
     |<----preview-----  (继续本地预览)                       |
     |                          |                           |
  [松开鼠标]                      |                           |
     |                    等待Server确认                      |
     |                          |<--broadcast_update()-----|
     |                    用Server值                         |
     |                    替换预览值                          |
     |<----finalize----  commit_transform()                 |
     |                          |                           |
```

### 4. 节点图执行流程

```
用户 (Editor)                  Editor                  Server
  |                             |                        |
[编辑节点图]                     |                        |
  |                        保存到本地JSON                 |
  |                             |                        |
[点击Execute]                    |                        |
  |                     serialize_graph()                |
  |                             |--execute_graph()----->|
  |                             |                   解析图
  |                             |                   构建Runtime
  |                             |                   开始执行
  |                             |<--ack--------------|  |
  |                             |               (执行中...)
  |                             |                        |
  |                             |<--simulation_update()--|
  |<--渲染更新---  更新视口显示    |                 (持续推送)
  |                             |<--simulation_update()--|
  |<--渲染更新---               |                        |
  |                             |                        |
  |                             |<--execution_finished---|
  |<--显示完成---               |                        |
```

---


## 关键设计优势

### 1. **真正的Python-First**
- Python完全控制场景和逻辑
- Editor是可选的，不影响核心功能
- 支持大规模离线仿真和渲染

### 3. **开发体验优化**
- GUI简化参数调整（比纯代码方便）
- 节点图可视化编辑（比JSON方便）
- 实时可视化调试（比盲跑方便）

### 4. **性能优化**
- 增量更新减少数据传输
- 乐观更新提升交互响应性
- 高频数据流使用高效序列化

### 5. **灵活的工作流**
```python
# 工作流 A: 纯代码（适合批量任务）
for config in configs:
    scene = rbc.Scene()
    scene.load(config.scene_file)
    results = scene.execute_node_graph(config.graph)
    results.save(config.output)

# 工作流 B: GUI辅助（适合开发调试）
scene = rbc.Scene()
editor = rbc.launch_editor()
# 在Editor中调整参数、编辑节点图
# 保存后再用工作流A批量运行

# 工作流 C: 混合（代码+GUI）
scene = rbc.Scene()
scene.load("base_scene.rbc")

# 代码创建一些实体
for i in range(100):
    obj = scene.create_entity(f"Object_{i}")
    scene.add_component(obj, rbc.TransformComponent(position=[i, 0, 0]))

# 启动Editor查看和微调
editor = rbc.launch_editor()
input("Press Enter to continue after editing...")

# 继续用代码执行
scene.execute_node_graph(graph)
```

---

## 总结

修改后的设计核心要点：

1. ✅ **Python是唯一真实来源**：场景数据、资源、状态全部在Python端
2. ✅ **Editor是可选工具**：调试器和可视化工具，不是必需品
3. ✅ **命令模式交互**：Editor通过命令与Server交互，不直接修改数据
4. ✅ **无头模式支持**：完全可以不启动Editor进行离线仿真
5. ✅ **GUI辅助开发**：节点图编辑、参数调整、实时可视化提升开发效率
6. ✅ **单向数据流**：Server → Editor推送更新，Editor → Server发送命令
7. ✅ **代码优先**：所有功能都可以用Python代码完成，GUI只是便利工具

这个架构真正体现了"Python-First"的理念，同时保留了Editor作为强大调试工具的价值！


## Editor Architecture Refactoring (Qt + Custom Renderer)

为了提高系统的可扩展性和解耦Qt UI与底层Renderer，我们采用了如下架构：

### 核心组件

1.  **EditorEngine (Runtime Core)**
    - **职责**: 管理应用程序的全局生命周期，持有 `rbc::App` (渲染后端) 和 `luisa::compute::Context`。
    - **模式**: Singleton (单例)。
    - **接口**: 实现了 `IRenderer` 接口，作为 UI 和 渲染器 之间的桥梁。
    - **生命周期**: 在 `main` 函数最开始初始化，在 `QApplication` 退出后销毁。

2.  **ViewportWidget (UI Component)**
    - **职责**: 封装了 `RhiWindow` (Qt RHI 窗口) 和 `QWidget` 容器。负责将 3D 渲染窗口嵌入到 Qt 布局中，并处理输入事件转发。
    - **复用性**: 作为一个独立的 `QWidget`，可以被放置在 `MainWindow` 的任意位置，也可以实例化多个。
    - **依赖**: 依赖 `IRenderer` (通常由 `EditorEngine` 提供) 来进行实际的渲染调用。

3.  **MainWindow (Main Frame)**
    - **职责**: 负责编辑器的主界面布局 (Docking, Menus, Toolbars)。
    - **集成**: 不再手动管理 Renderer 的初始化，而是直接使用 `ViewportWidget`。

### 生命周期 (Lifecycle)

1.  **Startup**:
    - `EditorEngine::instance().init()`: 初始化 LuisaCompute Context 和 Backend Device。
    - `QApplication` 初始化。
    - `MainWindow` 创建 -> `ViewportWidget` 创建 -> `RhiWindow` 创建。
    - `RhiWindow` 初始化 `QRhi` (DirectX/Vulkan/Metal)。

2.  **Runtime**:
    - Qt 事件循环处理 UI 事件。
    - `RhiWindow` 在每一帧调用 `IRenderer::update()` 和 `IRenderer::get_present_texture()` 进行渲染和显示。
    - 输入事件 (Key/Mouse) 由 `ViewportWidget` 捕获并转发给 `RhiWindow`，再传递给 `EditorEngine`。

3.  **Shutdown**:
    - `QApplication::exec()` 返回。
    - `MainWindow` 析构 -> `ViewportWidget` 析构 -> `RhiWindow` 释放 SwapChain 并析构。
    - **关键点**: UI 组件必须在 Engine 销毁前析构，以确保 RHI 资源 (SwapChain, Textures) 能在 Device 销毁前安全释放。
    - `EditorEngine::instance().shutdown()`: 销毁 Backend Device 和 Context。

### 优势

- **解耦**: `main.cpp` 不再包含大量的初始化胶水代码。
- **稳定性**: 明确了销毁顺序，避免了 Device Lost 或 资源泄露导致的 Crash。
- **可维护性**: `ViewportWidget` 可以在任何地方使用，方便未来支持多视口 (Multi-viewport)。
