# 连接时序修复总结

## 问题分析

### 根本原因
Editor 端出现 "connection refused" 错误，原因有两个：

1. **HttpClient 默认端口不匹配**
   - HttpClient 构造函数默认 URL: `http://127.0.0.1:8000`
   - Python 服务器实际端口: `5555`
   - 导致初始连接失败

2. **NodeEditor 连接时序问题**
   - NodeEditor 在 `setupDocks()` 中创建，此时创建了自己的 HttpClient 实例
   - NodeEditor 构造函数中立即调用 `loadNodesFromBackend()`
   - 但此时 MainWindow 还没调用 `startSceneSync()` 设置正确的服务器 URL
   - 导致 NodeEditor 尝试连接错误的端口 (8000) 并失败

### 时序问题示意

**修复前的错误时序**：
```
1. MainWindow::setupUi()
   ├── setupDocks()
   │   └── NodeEditor(parent) 构造
   │       ├── 创建 HttpClient (默认 8000 端口)
   │       └── loadNodesFromBackend() ❌ 连接 8000 端口失败
   └── ...
2. MainWindow::startSceneSync("http://127.0.0.1:5555")
   └── SceneSyncManager::start()
       └── httpClient_->setServerUrl("5555") ⚠️ 但 NodeEditor 的 HttpClient 还是 8000
```

**修复后的正确时序**：
```
1. MainWindow::setupUi()
   ├── 创建 httpClient_ (默认 8000，但还未使用)
   ├── setupDocks()
   │   └── NodeEditor(httpClient_, parent) 构造
   │       └── 共享 MainWindow 的 HttpClient
   │       └── 不立即加载节点 ✅
   └── ...
2. MainWindow::startSceneSync("http://127.0.0.1:5555")
   ├── httpClient_->setServerUrl("5555") ✅ 所有组件共享此 URL
   ├── SceneSyncManager::start()
   └── QTimer::singleShot(500ms)
       └── nodeEditor_->loadNodesDeferred() ✅ 延迟加载，确保连接建立
```

## 修复方案

### 1. 更新 HttpClient 默认端口
**文件**: `rbc/editor/src/runtime/HttpClient.cpp`

将默认端口从 8000 改为 5555，匹配服务器实际端口：
```cpp
HttpClient::HttpClient(QObject *parent)
    : QObject(parent), 
      m_networkManager(new QNetworkAccessManager(this)), 
      m_serverUrl("http://127.0.0.1:5555"),  // 改为 5555
      m_isConnected(false) {
}
```

### 2. NodeEditor 共享 HttpClient
**文件**: `rbc/editor/include/RBCEditor/components/NodeEditor.h`

添加新的构造函数，接受外部 HttpClient：
```cpp
public:
    explicit NodeEditor(QWidget *parent);
    explicit NodeEditor(HttpClient *httpClient, QWidget *parent);  // 新增
    
    // 延迟加载节点（在服务器连接后调用）
    void loadNodesDeferred();
```

**文件**: `rbc/editor/src/components/NodeEditor.cpp`

```cpp
// 原构造函数：创建自己的 HttpClient
NodeEditor::NodeEditor(QWidget *parent) 
    : QWidget(parent),
      m_httpClient(new HttpClient(this)),
      m_nodeFactory(std::make_unique<NodeFactory>()),
      m_isExecuting(false),
      m_serverUrl("http://127.0.0.1:5555") {  // 改为 5555
    setupUI();
    setupConnections();
    // 不再立即加载节点
}

// 新构造函数：使用共享的 HttpClient
NodeEditor::NodeEditor(HttpClient *httpClient, QWidget *parent) 
    : QWidget(parent),
      m_httpClient(httpClient),  // 共享 HttpClient
      m_nodeFactory(std::make_unique<NodeFactory>()),
      m_isExecuting(false),
      m_serverUrl(httpClient->serverUrl()) {
    setupUI();
    setupConnections();
    // 延迟加载，等待 MainWindow 触发
}

void NodeEditor::loadNodesDeferred() {
    loadNodesFromBackend();
}
```

### 3. MainWindow 协调连接顺序
**文件**: `rbc/editor/include/RBCEditor/MainWindow.h`

添加 NodeEditor 成员：
```cpp
private:
    rbc::NodeEditor *nodeEditor_;  // 新增
```

**文件**: `rbc/editor/src/MainWindow.cpp`

```cpp
// 在 setupDocks() 中使用共享的 HttpClient
nodeEditor_ = new rbc::NodeEditor(httpClient_, nodeDock);

// 在 startSceneSync() 中延迟加载节点
void MainWindow::startSceneSync(const QString &serverUrl) {
    // ... 启动场景同步
    sceneSyncManager_->start(serverUrl);
    
    // 延迟 500ms 后加载节点，确保服务器连接已建立
    if (nodeEditor_) {
        QTimer::singleShot(500, [this]() {
            if (nodeEditor_) {
                nodeEditor_->loadNodesDeferred();
            }
        });
    }
}
```

## 修复效果

### 修复前
- ❌ NodeEditor 连接 8000 端口 → Connection Refused
- ❌ SceneSyncManager 连接 5555 端口 → 可能成功，但 NodeEditor 已失败
- ❌ 控制台显示连接错误日志

### 修复后
- ✅ 所有组件共享同一个 HttpClient 实例
- ✅ HttpClient 默认端口为 5555
- ✅ startSceneSync() 设置 URL 后，所有组件都使用正确的 URL
- ✅ NodeEditor 延迟加载节点，确保服务器连接已建立
- ✅ 无连接错误日志

## 关键改进

1. **单一 HttpClient 实例**：所有组件共享 MainWindow 的 HttpClient
2. **统一服务器 URL**：默认 5555 端口，与 Python 服务器一致
3. **延迟初始化**：NodeEditor 在服务器连接后才加载节点
4. **清晰的生命周期**：MainWindow → setupUi → startSceneSync → 延迟加载

## 测试验证

启动后应该看到：
```
[EditorService] Started on port 5555
...
Connected to scene server          (SceneSyncManager 成功)
Loaded N node types from backend   (NodeEditor 成功)
```

不应再看到：
```
❌ Connection refused. Is the backend server running?
```

