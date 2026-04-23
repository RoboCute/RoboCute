---
name: editor
description: C++ editor development guidelines for RoboCute editor (rbc/editor). Covers plugin architecture, service system, MVVM ViewModels, UI contributions (QML/Native views, menus, toolbars), QtNodes node editor, viewport rendering, and Qt lifecycle management.
triggers:
  - file_types: [".cpp", ".h", ".hpp"]
    path_patterns: ["rbc/editor/**/*"]
---

# RoboCute Editor Skill

Guidelines for developing the RoboCute editor (`rbc/editor`), a Qt/QML-based plugin-extensible editor with MVVM architecture, node-based graph editing, and RHI viewport rendering.

## Architecture Overview

The editor follows a layered architecture:

1. **Runtime** (`rbc/editor/runtime/`) — Core framework: plugin system, services, MVVM, UI management
2. **Plugins** (`rbc/editor/plugins/`) — Feature plugins loaded as DLLs: Connection, Project, NodeEditor, Layout, etc.
3. **Editor Entry** (`rbc/editor/editor/`) — `dll_main()` bootstrap and shutdown sequence

## Plugin System

### Creating a Plugin

All plugins implement `IEditorPlugin` and export a factory function.

```cpp
// MyPlugin.h
#pragma once
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"

namespace rbc {

class MyPlugin : public IEditorPlugin {
    Q_OBJECT
public:
    explicit MyPlugin(QObject *parent = nullptr);
    ~MyPlugin() override;

    // === Lifecycle ===
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    // === Metadata ===
    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }

    // === UI Contributions ===
    QList<ViewContribution> view_contributions() const override;
    QList<NativeViewContribution> native_view_contributions() const override { return {}; }
    QList<MenuContribution> menu_contributions() const override;
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }

    // === ViewModel ===
    void register_view_models(QQmlEngine *engine) override;
    QObject *getViewModel(const QString &viewId) override;

    // === Factory Helpers ===
    static QString staticPluginId() { return "com.robocute.myplugin"; }
    static QString staticPluginName() { return "My Plugin"; }

private:
    PluginContext *context_ = nullptr;
    MyViewModel *viewModel_ = nullptr;
    QList<MenuContribution> menuContributions_;
};

}// namespace rbc
```

```cpp
// MyPlugin.cpp
#include "MyPlugin.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"
#include <QQmlEngine>
#include <QDebug>

namespace rbc {

MyPlugin::MyPlugin(QObject *parent) : IEditorPlugin(parent) {}
MyPlugin::~MyPlugin() {}

bool MyPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "MyPlugin::load: context is null";
        return false;
    }
    context_ = context;

    // Get a service
    auto *projectService = context->getService<IProjectService>();
    if (!projectService) {
        qWarning() << "MyPlugin::load: ProjectService not available";
        return false;
    }

    // Create ViewModel
    viewModel_ = new MyViewModel(projectService, this);

    // Build menus
    buildMenuContributions();

    qDebug() << "MyPlugin loaded successfully";
    return true;
}

bool MyPlugin::unload() {
    if (viewModel_) {
        viewModel_->deleteLater();
        viewModel_ = nullptr;
    }
    menuContributions_.clear();
    context_ = nullptr;
    qDebug() << "MyPlugin unloaded";
    return true;
}

bool MyPlugin::reload() {
    if (!unload()) return false;
    return load(context_);
}

QList<ViewContribution> MyPlugin::view_contributions() const {
    ViewContribution view;
    view.viewId = "myplugin.view";
    view.title = "My View";
    view.qmlSource = "MyView.qml";
    view.qmlHotDir = STR(RBCE_PLUGIN_PATH);
    view.dockArea = "Right";
    view.preferredSize = "300,400";
    view.closable = true;
    view.movable = true;
    return {view};
}

QList<MenuContribution> MyPlugin::menu_contributions() const {
    return menuContributions_;
}

void MyPlugin::buildMenuContributions() {
    MenuContribution menu;
    menu.menuPath = "Tools/MyPlugin";
    menu.actionText = "Run Action";
    menu.actionId = "myplugin.run";
    menu.shortcut = "Ctrl+Shift+M";
    menu.callback = [this]() {
        if (viewModel_) viewModel_->runAction();
    };
    menuContributions_.append(menu);
}

void MyPlugin::register_view_models(QQmlEngine *engine) {
    if (!engine) return;
    qmlRegisterType<MyViewModel>("RoboCute.MyPlugin", 1, 0, "MyViewModel");
}

QObject *MyPlugin::getViewModel(const QString &viewId) {
    if (viewId == "myplugin.view" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

// Export factory function for dynamic loading
IPluginFactory *createPluginFactory() {
    return new PluginFactory<MyPlugin>();
}

}// namespace rbc
```

### Plugin Loading (Dynamic DLL)

Plugins are loaded via `EditorPluginManager::loadPluginFromDLL("RBCE_MyPlugin")`.
The DLL must export:

```cpp
IPluginFactory *createPluginFactory();
```

Use `PluginFactory<T>` template for built-in plugins:

```cpp
pluginManager.registerPlugin<MyPlugin>();
pluginManager.loadPlugin(MyPlugin::staticPluginId());
```

### Plugin Lifecycle Rules

1. `load(context)` — initialize, get services, create ViewModels, build contributions
2. `unload()` — clean up ViewModels (use `deleteLater()`), clear contributions, null out service pointers
3. `reload()` — save state, `unload()`, `load(context)`, restore state
4. Never delete services in `unload()` — they are shared and managed by PluginManager

## Service System

### Creating a Service

Services are `QObject`-derived and extend `IService`.

```cpp
// IMyService.h
#pragma once
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

class IMyService : public IService {
    Q_OBJECT
public:
    explicit IMyService(QObject *parent = nullptr) : IService(parent) {}
    virtual ~IMyService() = default;

    QString serviceId() const override { return "com.robocute.my_service"; }

    virtual QString getData() const = 0;
    virtual void setData(const QString &data) = 0;

signals:
    void dataChanged();
};

}// namespace rbc
```

```cpp
// MyService.h
#pragma once
#include "IMyService.h"

namespace rbc {

class MyService : public IMyService {
    Q_OBJECT
public:
    explicit MyService(QObject *parent = nullptr);
    ~MyService() override;

    QString getData() const override { return data_; }
    void setData(const QString &data) override;

private:
    QString data_;
};

}// namespace rbc
```

### Registering Services

Services are registered before plugins load:

```cpp
auto *myService = new MyService();
EditorPluginManager::instance().registerService(myService);
```

Plugins access services via `PluginContext`:

```cpp
auto *service = context->getService<IMyService>();
```

Or via `PluginManager` directly:

```cpp
auto *service = EditorPluginManager::instance().getService<IMyService>();
```

### Service Interface Best Practices

- Define an `I*` interface in `runtime/include/RBCEditorRuntime/services/`
- Implement in `runtime/src/services/`
- Use `QObject` signals for change notifications
- Use inline destructor in interface headers to avoid linkage issues
- Service lifetime exceeds plugins; never store raw pointers to ViewModels in services

## MVVM ViewModel

### Base Class

All ViewModels derive from `ViewModelBase`:

```cpp
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"

class MyViewModel : public ViewModelBase {
    Q_OBJECT
    Q_PROPERTY(QString displayText READ displayText NOTIFY displayTextChanged)

public:
    explicit MyViewModel(ISomeService *service, QObject *parent = nullptr);
    ~MyViewModel() override;

    QString displayText() const { return displayText_; }

    Q_INVOKABLE void doSomething();

signals:
    void displayTextChanged();

private:
    ISomeService *service_ = nullptr;
    QString displayText_;
};
```

### ViewModel Lifecycle

- `onActivate()` — called when view becomes visible
- `onDeactivate()` — called when view is hidden
- `onReload()` — called on QML hot reload

### ViewModel Cleanup

In the plugin's `unload()`, call `viewModel_->deleteLater()` (not `delete`).
In the ViewModel destructor, explicitly disconnect from service signals:

```cpp
MyViewModel::~MyViewModel() {
    if (service_) {
        QObject::disconnect(service_, nullptr, this, nullptr);
    }
    service_ = nullptr;
}
```

## UI Contributions

### ViewContribution (QML View)

For dockable QML views that support hot reload:

```cpp
ViewContribution view;
view.viewId = "myplugin.panel";
view.title = "My Panel";
view.qmlSource = "MyPanel.qml";
view.qmlHotDir = STR(RBCE_PLUGIN_PATH);  // hot reload source dir
view.dockArea = "Left";    // Left/Right/Top/Bottom/Center
view.preferredSize = "300,400";
view.closable = true;
view.movable = true;
```

### NativeViewContribution (QWidget View)

For native QWidget-based views (e.g., ViewportWidget, NodeEditor):

```cpp
NativeViewContribution contrib;
contrib.viewId = "myplugin.native_view";
contrib.title = "Native View";
contrib.dockArea = "Center";
contrib.closable = true;
contrib.movable = true;
contrib.floatable = true;
contrib.isExternalManaged = true;  // Plugin manages widget lifecycle
```

When `isExternalManaged = true`, `WindowManager` will not delete the widget during cleanup.

### MenuContribution

```cpp
MenuContribution menu;
menu.menuPath = "File/Export";       // Supports nested paths
menu.actionText = "Export Data";
menu.actionId = "myplugin.export";
menu.shortcut = "Ctrl+Shift+E";
menu.callback = [this]() {
    // Action logic
};
```

### ToolbarContribution

```cpp
ToolbarContribution tb;
tb.toolbarId = "main";
tb.toolbarName = "Main Toolbar";
tb.actionId = "myplugin.action";
tb.actionText = "Run";
tb.iconPath = ":/icons/run.png";
tb.callback = [this]() {
    // Action logic
};
```

## Node Editor (QtNodes)

The editor integrates `QtNodes` for data-flow graph editing.

### Dynamic Node Model

Nodes are created dynamically from JSON metadata:

```cpp
#include "RBCEditorRuntime/infra/nodes/DynamicNodeModel.h"

// Node metadata from backend
QJsonObject metadata;
metadata["node_type"] = "add";
metadata["display_name"] = "Add";
metadata["category"] = "Math";
metadata["inputs"] = QJsonArray{...};
metadata["outputs"] = QJsonArray{...};

auto node = std::make_unique<DynamicNodeModel>(metadata);
```

### NodeFactory

Register nodes from metadata:

```cpp
#include "RBCEditorRuntime/infra/nodes/NodeFactory.h"

NodeFactory factory;
factory.registerNodesFromMetadata(nodesMetadataArray);
```

### Custom Input Widgets

Implement `IInputWidgetCreator` for custom node input UI:

```cpp
#include "RBCEditorRuntime/infra/nodes/IInputWidgetCreator.h"

class MyWidgetCreator : public IInputWidgetCreator {
public:
    bool canCreate(const QJsonObject &inputDef) const override;
    QWidget *createWidget(const QJsonObject &inputDef, QWidget *parent) override;
    QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) override;
};
```

Register with `InputWidgetFactory::registerCreator()`.

## Viewport System

### ViewportPlugin

Manages rendering viewports. Renderer factory must be set by `EditorEngine` before creating viewports:

```cpp
viewportPlugin->setRendererFactory([](const ViewportConfig &config) -> IRenderer * {
    IRenderer *app;
    switch (config.type) {
        case ViewportType::Main: app = new VisApp(); break;
        default: app = new VisApp(); break;
    }
    app->init(programPath, backendName);
    return app;
});
```

### ViewportConfig

```cpp
ViewportConfig config;
config.viewportId = "viewport.main";
config.type = ViewportType::Main;
config.enableGizmos = true;
config.enableGrid = true;
config.enableSelection = true;
config.graphicsApi = QRhi::D3D12;

QString id = viewportPlugin->createViewport(config);
```

### IRenderer / RenderAppBase

Custom renderers derive from `RenderAppBase`:

```cpp
class MyRenderer : public RenderAppBase {
public:
    void update() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) override;
    void handle_cursor_position(luisa::float2 xy) override;
    uint64_t get_present_texture(uint width, uint height) override;
    RenderMode getRenderMode() const override { return RenderMode::Editor; }

protected:
    void on_init() override;
};
```

## Critical Lifecycle & Cleanup

The shutdown sequence in `dll_main()` is **critical** to avoid crashes:

```cpp
// 1. Cleanup WindowManager (hide, break QML references)
windowManager.cleanup();
QCoreApplication::processEvents(QEventLoop::AllEvents);

// WindowManager goes out of scope here, destroying all QQuickWidgets

// 2. Delete QQmlEngine BEFORE unloading plugins
pluginManager.setQmlEngine(nullptr);
QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
delete engine;
engine = nullptr;

// 3. Unload all plugins (DLLs still loaded, types valid)
pluginManager.unloadAllPlugins();

// 4. Clear service references
pluginManager.clearServices();

// 5. Shutdown EditorEngine
EditorEngine::instance().shutdown();
```

### QPointer Usage

Use `QPointer` for widgets that may be deleted by Qt's parent-child mechanism:

```cpp
struct EditorInstance {
    QPointer<QWidget> widget;  // Auto-nulls if Qt deletes it
    ViewModelBase *viewModel = nullptr;
};

if (instance->widget) {
    delete instance->widget.data();  // Safe check before delete
}
```

### Async Callback Safety

Use `QPointer` in lambdas capturing `this` for async operations:

```cpp
QPointer<MyService> self = this;
QObject::connect(reply, &QNetworkReply::finished, [self, reply]() {
    if (!self) {
        reply->deleteLater();
        return;  // Object was destroyed
    }
    // Safe to use self
    self->processReply(reply);
    reply->deleteLater();
});
```

## Include Conventions

Use the `RBCEditorRuntime/` prefix for runtime headers:

```cpp
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/services/IService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/ui/WindowManager.h"
```

Plugin-local headers use relative paths:

```cpp
#include "MyPlugin.h"
#include "MyViewModel.h"
```

## Export Macro

Runtime API uses `RBC_EDITOR_RUNTIME_API`:

```cpp
class RBC_EDITOR_RUNTIME_API MyClass : public QObject {
    // ...
};
```

## Testing

Tests use doctest framework in `rbc/editor/tests/`:

```cpp
#include "../_framework/test_util.h"

TEST_SUITE("EditorPlugin") {
    TEST_CASE("load_and_unload") {
        auto plugin = std::make_unique<MyPlugin>();
        PluginContext ctx(&EditorPluginManager::instance(), nullptr);
        CHECK(plugin->load(&ctx));
        CHECK(plugin->unload());
    }
}
```
