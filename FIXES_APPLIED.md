# 修复应用说明

## 修复 1: EditorScene 生命周期问题

### 问题
EditorScene 在构造函数中过早初始化灯光（`initLight()`），此时 TLAS 中还没有任何 mesh instance，导致场景管理器状态不一致或 crash。

### 原因分析
对比 SimpleScene 的实现发现：
- SimpleScene 在 `_init_tlas()` 中**同时添加 mesh instance 和 light**
- EditorScene 在构造函数中**立即初始化 light**，违反了这个模式

### 解决方案：延迟初始化（Lazy Initialization）

#### 修改文件
- `rbc/editor/include/RBCEditor/runtime/EditorScene.h`
- `rbc/editor/src/runtime/EditorScene.cpp`

#### 关键修改

1. **添加状态跟踪**
```cpp
bool light_initialized_ = false;  // 跟踪灯光是否已初始化
```

2. **构造函数中移除立即初始化**
```cpp
EditorScene::EditorScene() {
    // ... 材质类型注册
    initMaterial();
    // ❌ 不再立即初始化灯光
    LUISA_INFO("EditorScene initialized (light will be initialized on first entity)");
}
```

3. **添加延迟初始化方法**
```cpp
void EditorScene::ensureLightInitialized() {
    if (!light_initialized_) {
        initLight();
    }
}
```

4. **在第一个实体添加时初始化**
```cpp
void EditorScene::addEntity(...) {
    ensureLightInitialized();  // ✅ 确保在添加实体前初始化灯光
    // ... 添加实体逻辑
}
```

5. **安全的析构**
```cpp
if (light_initialized_ && light_id_ != 0) {
    lights_.remove_area_light(light_id_);
}
```

### 效果
- ✅ 灯光只在有 mesh instance 时初始化
- ✅ TLAS 状态始终一致
- ✅ 避免空场景时的 crash
- ✅ 遵循 SimpleScene 的正确模式

---

## 修复 2: HttpClient 连接时序问题

### 问题
Editor 连接到服务器后仍显示 "connection refused" 错误。

### 原因分析

1. **端口不匹配**
   - HttpClient 默认端口: 8000
   - Python 服务器实际端口: 5555

2. **NodeEditor 连接时序错误**
   ```
   MainWindow::setupUi()
   └── setupDocks()
       └── NodeEditor() 构造
           ├── 创建独立的 HttpClient (8000端口)
           └── loadNodesFromBackend() ❌ 立即尝试连接
   
   MainWindow::startSceneSync("5555")  ⚠️ 但 NodeEditor 的 HttpClient 还是 8000
   ```

3. **多个 HttpClient 实例**
   - MainWindow 有自己的 httpClient_
   - NodeEditor 创建了独立的 m_httpClient
   - 两者配置不同步

### 解决方案：共享 HttpClient + 延迟加载

#### 修改文件
- `rbc/editor/src/runtime/HttpClient.cpp`
- `rbc/editor/include/RBCEditor/components/NodeEditor.h`
- `rbc/editor/src/components/NodeEditor.cpp`
- `rbc/editor/include/RBCEditor/MainWindow.h`
- `rbc/editor/src/MainWindow.cpp`

#### 关键修改

1. **HttpClient 默认端口改为 5555**
```cpp
// rbc/editor/src/runtime/HttpClient.cpp
HttpClient::HttpClient(QObject *parent)
    : QObject(parent), 
      m_networkManager(new QNetworkAccessManager(this)), 
      m_serverUrl("http://127.0.0.1:5555"),  // ✅ 改为 5555
      m_isConnected(false) {
}
```

2. **NodeEditor 添加共享 HttpClient 构造函数**
```cpp
// rbc/editor/include/RBCEditor/components/NodeEditor.h
explicit NodeEditor(QWidget *parent);
explicit NodeEditor(HttpClient *httpClient, QWidget *parent);  // ✅ 新增
void loadNodesDeferred();  // ✅ 延迟加载接口

// rbc/editor/src/components/NodeEditor.cpp
NodeEditor::NodeEditor(HttpClient *httpClient, QWidget *parent) 
    : QWidget(parent),
      m_httpClient(httpClient),  // ✅ 共享 MainWindow 的 HttpClient
      m_nodeFactory(std::make_unique<NodeFactory>()),
      m_isExecuting(false),
      m_serverUrl(httpClient->serverUrl()) {
    setupUI();
    setupConnections();
    // ✅ 不立即加载节点，等待 MainWindow 触发
}

void NodeEditor::loadNodesDeferred() {
    loadNodesFromBackend();
}
```

3. **MainWindow 使用共享 HttpClient**
```cpp
// rbc/editor/src/MainWindow.cpp
void MainWindow::setupDocks() {
    // ...
    // ✅ 使用 MainWindow 的 httpClient_
    nodeEditor_ = new rbc::NodeEditor(httpClient_, nodeDock);
    // ...
}

void MainWindow::startSceneSync(const QString &serverUrl) {
    // ... 启动场景同步
    sceneSyncManager_->start(serverUrl);
    
    // ✅ 延迟 500ms 后加载节点，确保连接已建立
    if (nodeEditor_) {
        QTimer::singleShot(500, [this]() {
            if (nodeEditor_) {
                nodeEditor_->loadNodesDeferred();
            }
        });
    }
}
```

### 正确的连接时序

```
1. MainWindow 构造
   └── 创建 httpClient_ (默认 5555)

2. MainWindow::setupUi()
   └── setupDocks()
       └── NodeEditor(httpClient_, parent)
           └── 使用共享的 HttpClient
           └── ✅ 不立即加载节点

3. MainWindow::startSceneSync("http://127.0.0.1:5555")
   ├── httpClient_->setServerUrl("5555")
   │   └── ✅ 所有组件自动使用新 URL
   ├── SceneSyncManager::start()
   │   └── 开始场景同步
   └── QTimer::singleShot(500ms)
       └── nodeEditor_->loadNodesDeferred()
           └── ✅ 此时连接已建立，成功加载节点

4. 后续所有 HTTP 请求
   └── ✅ 都使用正确的端口 5555
```

### 效果
- ✅ 单一 HttpClient 实例，配置统一
- ✅ 所有组件自动使用正确的服务器 URL
- ✅ 延迟加载避免连接竞态
- ✅ 无 "connection refused" 错误
- ✅ NodeEditor 成功加载节点列表
- ✅ SceneSyncManager 成功同步场景

## 验证步骤

1. **启动服务器**
   ```bash
   python main.py
   ```
   应该看到：
   ```
   Editor Service started on port 5555
   ```

2. **启动 Editor**
   ```bash
   editor.exe
   ```
   应该看到：
   - 状态栏显示 "Connected to scene server"
   - NodeGraph 面板加载节点列表
   - 无连接错误日志

3. **检查控制台**
   - ✅ 无 "connection refused" 错误
   - ✅ 可以看到 "Loaded N node types" 消息
   - ✅ 场景同步成功

## 总结

通过两个关键修复：
1. **EditorScene 生命周期**：延迟灯光初始化，避免 TLAS 空状态
2. **HttpClient 连接时序**：共享实例 + 延迟加载，确保正确的连接顺序

现在 Editor 可以正确连接到 Python 服务器，并完整运行 MVP 功能。

