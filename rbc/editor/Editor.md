# RBC Editor 架构深度分析

## 1. C++ 项目与目标结构

### 目录层级
```
rbc/editor/
├── xmake.lua              # 根构建脚本，includes 子目录
├── CMakeLists.txt         # CMake 入口（仅 add_subdirectory）
├── editor/                # 可执行入口与模块 DLL
│   ├── main_entry.cpp     # 主入口：fiber scheduler + RuntimeStatic + PluginManager
│   ├── rbc_editor_module.cpp      # 正式版 dll_main（生产模块）
│   ├── rbc_editor_test_module.cpp # 测试版 dll_main（支持热更新、简化插件集）
│   └── xmake.lua          # 定义 rbc_editor（binary）、rbc_editor_module/test_module（shared）
├── runtime/               # 编辑器运行时框架（核心共享库）
│   ├── include/RBCEditorRuntime/  # 公共头文件（RBCEditorRuntime/ 前缀包含）
│   ├── src/               # 实现代码
│   ├── qml/               # 内置 QML 资源
│   ├── shaders/           # RHI 着色器（quad.vert/frag）
│   └── xmake.lua          # interface_target('rbc_editor_runtime', ...)
├── plugins/               # 功能插件 DLL（独立编译）
│   ├── connection_plugin/
│   ├── layout_plugin/
│   ├── node_editor_plugin/
│   └── project_plugin/
└── tests/                 # 测试套件
    ├── calculator/        # QtNodes 计算器 demo
    ├── func/              # 功能测试
    ├── mock/              # Mock 服务
    └── testbed/           # storybook / render / graph testbed
```

### 构建目标
| 目标 | 类型 | 职责 |
|------|------|------|
| `rbc_editor` | binary | 最小启动器，加载模块并调用 `dll_main` |
| `rbc_editor_module` | shared (DLL) | 正式编辑器启动逻辑（QApplication + 全插件） |
| `rbc_editor_test_module` | shared (DLL) | 测试启动逻辑（支持 `--enable_hot_reload`、F5 热重载） |
| `rbc_editor_runtime` | interface_target + shared | 运行时框架，提供插件系统、MVVM、Service、UI 管理、RHI 视口 |
| `RBCE_*Plugin` | shared (DLL) | 各功能插件，动态加载，导出 `createPluginFactory()` |

### 依赖链
- `rbc_editor` → `rbc_runtime`（核心运行时静态库）
- `rbc_editor_module` / `test_module` → `rbc_editor_runtime`（public deps）
- `rbc_editor_runtime` → `qt_node_editor` + `rbc_core` + `rbc_runtime`
- 插件 → `rbc_editor_runtime`

---

## 2. 生命周期与销毁顺序

### 2.1 启动序列（`dll_main`）
```
QApplication app(argc, argv)
  → EditorEngine::instance().init(argc, argv)    // 注册 Service、ViewportPlugin
  → QQmlEngine *engine = new QQmlEngine()        // 手动管理，无 parent
  → pluginManager.setQmlEngine(engine)
  → loadPluginFromDLL("RBCE_*Plugin")            // 加载各插件 DLL
  → WindowManager windowManager(&pluginManager)  // 作用域块内
  → LayoutService → registerService → initialize
  → applyMenuContributions                       // 菜单（非 LayoutService 管理）
  → layoutService->applyLayout("rbce.layout.scene_editing")
  → mainWindow->show()
  → app.exec()                                   // 主事件循环
```

### 2.2 关键销毁顺序（防崩溃）
```
// Phase 1: WindowManager 作用域内
windowManager.cleanup()                          // 1. 隐藏窗口、停止渲染
  → 清理所有 QQuickWidget 的 contextProperty("viewModel")
  → 释放外部 widget 引用（setParent(nullptr)，防 double-free）
  → 断开菜单 QAction 信号
QCoreApplication::processEvents(AllEvents)       // 处理未决事件
// WindowManager 析构：自动删除 main_window_ 及其子 QQuickWidget

// Phase 2: WindowManager 作用域外
QCoreApplication::processEvents(AllEvents)
pluginManager.setQmlEngine(nullptr)              // 2. 断 QML 引擎引用
delete engine; engine = nullptr                  // 3. 删除 QQmlEngine
  // 关键：此时 DLL 仍加载，qmlRegisterType 的 vtable 仍有效
QCoreApplication::processEvents(ExcludeUserInputEvents)

pluginManager.unloadAllPlugins()                 // 4. 卸载插件（DLL 安全卸载）
pluginManager.clearServices()                    // 5. 清空 service 引用 map
EditorEngine::instance().shutdown()              // 6. 关闭引擎
```

### 2.3 插件生命周期
```
registerFactory<T>() / loadPluginFromDLL()
  → loadPluginInternal()
    → new PluginContext(&pluginManager)
    → plugin->load(context)      // 初始化：获取 Service、创建 ViewModel、构建 Contributions
    → plugin->register_view_models(engine)
    → initializePlugin()
    → _plugins[id] = move(plugin)

unloadPlugin(id)
  → 检查反向依赖
  → plugin->unload()             // ViewModel::deleteLater()、清理 contributions
  → _plugins.erase(id)           // unique_ptr 释放 plugin 对象
  → 若动态库：unload_module() + _factories.erase(id)

reloadPlugin(id)
  → plugin->reload()             // 默认实现：unload() → load(context)
  → re-register_view_models(engine)
```

### 2.4 Service 生命周期
- **Service 存活周期 > Plugin 存活周期**
- `EditorEngine::init()` 中注册核心 Service（ProjectService、ConnectionService、StyleManager）
- 插件通过 `context->getService<T>()` 获取 Service 指针
- `unload()` 中仅置空指针，**禁止 delete Service**
- `clearServices()` 只清空 PluginManager 的引用 map，不 disconnect 信号（避免访问已删除对象）

### 2.5 ViewModel 生命周期
- `load()` 中 `new MyViewModel(service, this)`（parent 为 plugin）
- `unload()` 中 `viewModel_->deleteLater()`，**禁止直接 delete**
- ViewModel 析构中必须 `disconnect(service_, nullptr, this, nullptr)`
- QML 通过 `quickWidget->rootContext()->setContextProperty("viewModel", viewModel)` 绑定

### 2.6 RHI 视口生命周期
- `ViewportPlugin::createViewport(config)` → `new ViewportWidget(renderer, api)`
- `ViewportWidget` 包含 `RhiWindow`（QWindow）+ `ViewportContainerWidget`（QWidget 容器）
- `RhiWindow::exposeEvent()` 触发 `init()` + `resizeSwapChain()`
- `RhiWindow::render()` 每帧调用 `renderer->update()` + QRhi 全屏 quad 绘制
- `ViewportWidget::~ViewportWidget()` 先 `releaseSwapChain()`，再删容器

---

## 3. 编码入口与工作流程

### 3.1 开发一个新插件的标准流程
```cpp
// 1. 继承 IEditorPlugin
class MyPlugin : public IEditorPlugin {
    Q_OBJECT
public:
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;
    QString id() const override { return staticPluginId(); }
    QList<ViewContribution> view_contributions() const override;
    QList<MenuContribution> menu_contributions() const override;
    void register_view_models(QQmlEngine *engine) override;
    QObject *getViewModel(const QString &viewId) override;
    static QString staticPluginId() { return "com.robocute.myplugin"; }
};

// 2. 实现 load：获取 Service → 创建 ViewModel → 构建 UI Contributions
bool MyPlugin::load(PluginContext *context) {
    context_ = context;
    auto *svc = context->getService<IProjectService>();
    viewModel_ = new MyViewModel(svc, this);
    buildMenuContributions();
    return true;
}

// 3. 实现 unload：deleteLater ViewModel、清理指针
bool MyPlugin::unload() {
    if (viewModel_) { viewModel_->deleteLater(); viewModel_ = nullptr; }
    context_ = nullptr;
    return true;
}

// 4. 导出工厂函数
IPluginFactory *createPluginFactory() {
    return new PluginFactory<MyPlugin>();
}
```

### 3.2 UI 贡献类型
| 类型 | 结构 | 管理方 | 热重载 |
|------|------|--------|--------|
| `ViewContribution` | QML + ViewModel | `WindowManager::createDockableView()` | 支持（`qmlHotDir`） |
| `NativeViewContribution` | QWidget（如 ViewportWidget） | `WindowManager::createDockableView()` | 不支持 |
| `MenuContribution` | 菜单路径 + callback | `WindowManager::applyMenuContributions()` | N/A |
| `ToolbarContribution` | 工具栏 + callback | `WindowManager` / `LayoutService` | N/A |

### 3.3 布局系统
- `LayoutService` 管理 dock 布局配置文件（`ui/layout/*.json`）
- 启动时 `layoutService->applyLayout("rbce.layout.scene_editing")`
- 布局中包含 viewId → `LayoutService` 向各插件查询 `getViewModel(viewId)` / `getNativeWidget(viewId)`
- 找不到 viewId 时创建占位符

### 3.4 渲染管线入口
```
RhiWindow::render()
  → renderer->update()               // VisApp::update()：相机、交互、LC 渲染
  → QRhi beginFrame → cmdBuffer
  → beginPass(fullscreen quad)
  → draw(3)                          // 绘制 presenter texture
  → endPass → endFrame
  → requestUpdate()                  // 请求下一帧
```
- `IRenderer` 接口：`init()` / `update()` / `handle_key()` / `handle_mouse()` / `get_present_texture()`
- `RenderAppBase` 实现 LuisaCompute 设备初始化与 QRhi D3D12 Interop
- `VisApp` 实现编辑器主视口：相机控制 + ClickManager 交互

---

## 4. 跨线程回调到 UI 渲染循环的方法

### 4.1 核心机制
Editor 的 UI 与渲染运行在 **Qt 主线程**（`QApplication::exec()`）。任何其他线程（工作线程、LC 计算线程、网络线程）要回调到 UI/渲染循环，必须使用 **Qt 的线程队列机制**：

**推荐方式：`QMetaObject::invokeMethod` + `Qt::QueuedConnection`**
```cpp
// 目标对象必须位于 UI 线程（有 QThread affinity）
QObject *uiObject = someViewModel;  // 或 WindowManager、ViewportWidget 等

// 从任意线程安全投递到 UI 线程
QMetaObject::invokeMethod(uiObject, [uiObject]() {
    // 此 lambda 在 UI 线程执行
    uiObject->setProperty("foo", 42);
}, Qt::QueuedConnection);
```

**替代方式：`QTimer::singleShot(0, target, lambda)`**
```cpp
QTimer::singleShot(0, targetObject, [targetObject]() {
    // 0ms 延迟，队列到 UI 线程事件循环
    targetObject->doSomethingOnUIThread();
});
```

### 4.2 在 Render Loop 中插入回调
若需在每帧渲染前/后执行自定义逻辑（如将计算结果同步到 UI），推荐以下模式：

```cpp
// 1. 定义一个 QObject 派生的 RenderCallbackBridge（活在 UI 线程）
class RenderCallbackBridge : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void()>;
    void postCallback(Callback cb) {
        QMutexLocker lock(&mutex);
        pendingCallbacks.append(std::move(cb));
    }
    void executePending() {
        QList<Callback> callbacks;
        {
            QMutexLocker lock(&mutex);
            callbacks = std::move(pendingCallbacks);
        }
        for (auto &cb : callbacks) cb();
    }
private:
    QMutex mutex;
    QList<Callback> pendingCallbacks;
};

// 2. 在自定义 Renderer::update() 中调用
void MyRenderer::update() {
    // ... 原有更新逻辑 ...
    
    // 执行所有从其他线程投递过来的回调
    renderBridge->executePending();
}

// 3. 从任意线程投递回调
renderBridge->postCallback([textureHandle]() {
    // 此代码将在下一帧 UI 渲染线程执行
    // 可安全操作 QRhi 资源、QML 属性、OpenGL/D3D12 对象
});
```

### 4.3 利用现有基础设施（信号槽 + QueuedConnection）
```cpp
// Service 或 ViewModel 中定义信号
class MyService : public QObject {
    Q_OBJECT
signals:
    void dataReady(QVariant data);  // 跨线程信号
};

// 连接时显式指定 QueuedConnection
QObject::connect(myService, &MyService::dataReady,
                 myViewModel, &MyViewModel::onDataReady,
                 Qt::QueuedConnection);

// 从工作线程 emit 信号，自动队列到 UI 线程执行槽函数
emit myService->dataReady(result);
```

### 4.4 注意事项
1. **目标对象必须有 UI 线程 affinity**：`QObject::thread()` 必须是 `QApplication` 所在线程。
2. **不要在工作线程直接操作 QQuickWidget / QRhi / QML**：会导致 crash 或 `atomic lockRelaxed` 访问冲突。
3. **若回调需要返回值**：使用 `QMetaObject::invokeMethod` 的 `Qt::BlockingQueuedConnection`（仅限工作线程 → UI 线程，且需确保 UI 线程未阻塞）。
4. **ViewportWidget / RhiWindow 的事件转发**：`ViewportContainerWidget` 通过 `QCoreApplication::sendEvent(_rhi_window, event)` 将 QWidget 鼠标/键盘事件转发给 `RhiWindow`（QWindow），所有事件处理均在 UI 线程完成。
