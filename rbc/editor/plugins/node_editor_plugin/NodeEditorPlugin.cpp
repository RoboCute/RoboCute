#include "NodeEditorPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"

// #include "RBCEditorRuntime/components/NodeEditor.h"
#include "RBCEditorRuntime/ui/NodeEditor.h"
// #include "RBCEditorRuntime/runtime/HttpClient.h"

#include <QDebug>

namespace rbc {

// ============================================================================
// NodeEditorViewModel Implementation
// ============================================================================

NodeEditorViewModel::NodeEditorViewModel(
    const NodeEditorConfig &config,
    NodeEditor *editor,
    QObject *parent)
    : ViewModelBase(parent), config_(config), editor_(editor), serverUrl_(config.serverUrl) {

    if (!editor_) {
        qWarning() << "NodeEditorViewModel: editor is null for:" << config.editorId;
    }

    if (editor_) {
        editor_->loadNodesDeferred();
    }
}

NodeEditorViewModel::~NodeEditorViewModel() {
    editor_ = nullptr;
}

void NodeEditorViewModel::setServerUrl(const QString &url) {
    if (serverUrl_ != url) {
        serverUrl_ = url;
        emit serverUrlChanged();
        qDebug() << "NodeEditorViewModel:" << config_.editorId << "server URL changed:" << url;
    }
}

void NodeEditorViewModel::connectToServer() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "connectToServer";
    if (editor_) {
        editor_->loadNodesDeferred();
    }
}

void NodeEditorViewModel::disconnectFromServer() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "disconnectFromServer";
    // TODO: Implement disconnect logic
    connected_ = false;
    statusText_ = "Disconnected";
    emit connectedChanged();
    emit statusTextChanged();
}

void NodeEditorViewModel::executeGraph() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "executeGraph";
    // The NodeEditor handles execution internally
    // We can trigger it via a slot if needed
}

void NodeEditorViewModel::refreshNodes() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "refreshNodes";
    if (editor_) {
        editor_->loadNodesDeferred();
    }
}

void NodeEditorViewModel::newGraph() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "newGraph";
    // TODO: Trigger new graph action on editor
}

void NodeEditorViewModel::saveGraph() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "saveGraph";
    // TODO: Trigger save graph action on editor
}

void NodeEditorViewModel::loadGraph() {
    qDebug() << "NodeEditorViewModel:" << config_.editorId << "loadGraph";
    // TODO: Trigger load graph action on editor
}

void NodeEditorViewModel::onConnectionStatusChanged(bool connected) {
    if (connected_ != connected) {
        connected_ = connected;
        statusText_ = connected ? "Connected" : "Disconnected";
        emit connectedChanged();
        emit statusTextChanged();
    }
}

void NodeEditorViewModel::onExecutionStateChanged(bool executing) {
    if (executing_ != executing) {
        executing_ = executing;
        emit executingChanged();
    }
}

// ============================================================================
// NodeEditorPlugin Implementation
// ============================================================================

NodeEditorPlugin::NodeEditorPlugin(QObject *parent)
    : IEditorPlugin(parent) {
    qDebug() << "NodeEditorPlugin created";
}

NodeEditorPlugin::~NodeEditorPlugin() {
    destroyAllNodeEditors();
    qDebug() << "NodeEditorPlugin destroyed";
}

bool NodeEditorPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "NodeEditorPlugin::load: context is null";
        return false;
    }

    context_ = context;

    // Create shared HttpClient for node editors
    // httpClient_ = new HttpClient(this);
    // httpClient_->setServerUrl("http://127.0.0.1:5555");

    // Create default node editor
    createDefaultNodeEditor();

    // Build menu contributions
    buildMenuContributions();

    qDebug() << "NodeEditorPlugin loaded successfully";
    return true;
}

bool NodeEditorPlugin::unload() {
    qDebug() << "NodeEditorPlugin::unload";
    destroyAllNodeEditors();
    registeredContributions_.clear();
    menuContributions_.clear();
    context_ = nullptr;
    qDebug() << "NodeEditorPlugin unloaded";
    return true;
}

bool NodeEditorPlugin::reload() {
    qDebug() << "NodeEditorPlugin::reload";
    return true;
}

void NodeEditorPlugin::createDefaultNodeEditor() {
    // Create main node editor
    NodeEditorConfig mainConfig;
    mainConfig.editorId = "node_editor.main";
    mainConfig.serverUrl = "http://127.0.0.1:5555";
    mainConfig.autoConnect = true;

    mainEditorId_ = createNodeEditor(mainConfig);

    if (!mainEditorId_.isEmpty()) {
        // Register to contributions
        NativeViewContribution mainContrib;
        mainContrib.viewId = mainConfig.editorId;
        mainContrib.title = "Node Editor";
        mainContrib.dockArea = "Bottom";     // Default dock area, layout will override
        mainContrib.isExternalManaged = true;// Plugin manages widget lifecycle
        mainContrib.closable = true;
        mainContrib.movable = true;
        mainContrib.floatable = true;

        registeredContributions_.append(mainContrib);

        qDebug() << "NodeEditorPlugin: Created main node editor:" << mainEditorId_;
    }
}

QString NodeEditorPlugin::createNodeEditor(const NodeEditorConfig &config) {
    if (editors_.contains(config.editorId)) {
        qWarning() << "NodeEditorPlugin::createNodeEditor: Editor already exists:" << config.editorId;
        return QString();
    }

    auto *instance = new NodeEditorInstance();
    instance->config = config;

    // Create the NodeEditor widget with shared HttpClient
    instance->widget = new NodeEditor(nullptr);
    // Create ViewModel
    instance->viewModel = new NodeEditorViewModel(config, instance->widget.data(), nullptr);

    // Connect HttpClient signals to ViewModel
    // connect(httpClient_, &HttpClient::connectionStatusChanged,
    //         instance->viewModel, &NodeEditorViewModel::onConnectionStatusChanged);

    // Auto-connect if configured
    if (config.autoConnect) {
        // Defer the connection to allow the widget to be fully set up
        QMetaObject::invokeMethod(instance->widget.data(), "loadNodesDeferred", Qt::QueuedConnection);
    }

    editors_.insert(config.editorId, instance);

    emit nodeEditorCreated(config.editorId);
    qDebug() << "NodeEditorPlugin: Created node editor:" << config.editorId;

    return config.editorId;
}

bool NodeEditorPlugin::destroyNodeEditor(const QString &editorId) {
    auto it = editors_.find(editorId);
    if (it == editors_.end()) {
        qWarning() << "NodeEditorPlugin::destroyNodeEditor: Editor not found:" << editorId;
        return false;
    }

    NodeEditorInstance *instance = it.value();

    // Delete Widget
    // 使用 QPointer 检查 widget 是否仍然存在
    // 如果 Qt 已经删除了 widget（例如通过 parent-child 机制），QPointer 会变成 nullptr
    if (instance->widget) {
        qDebug() << "NodeEditorPlugin::destroyNodeEditor: Deleting widget for:" << editorId;
        delete instance->widget.data();
        // QPointer 会自动变成 nullptr，无需手动设置
    } else {
        qDebug() << "NodeEditorPlugin::destroyNodeEditor: Widget already deleted for:" << editorId;
    }

    // Delete instance (destructor cleans up viewModel)
    delete instance;

    editors_.erase(it);

    // Clear main reference if this was the main editor
    if (editorId == mainEditorId_) {
        mainEditorId_.clear();
    }

    // Remove from contributions
    registeredContributions_.erase(
        std::remove_if(
            registeredContributions_.begin(), registeredContributions_.end(),
            [&editorId](const NativeViewContribution &c) {
                return c.viewId == editorId;
            }),
        registeredContributions_.end());

    emit nodeEditorDestroyed(editorId);
    qDebug() << "NodeEditorPlugin: Destroyed node editor:" << editorId;

    return true;
}

void NodeEditorPlugin::destroyAllNodeEditors() {
    QStringList ids = editors_.keys();
    for (const QString &id : ids) {
        destroyNodeEditor(id);
    }
}

NodeEditorInstance *NodeEditorPlugin::getNodeEditor(const QString &editorId) {
    return editors_.value(editorId, nullptr);
}

QStringList NodeEditorPlugin::allEditorIds() const {
    return editors_.keys();
}

NodeEditorInstance *NodeEditorPlugin::mainNodeEditor() const {
    if (mainEditorId_.isEmpty()) {
        return nullptr;
    }
    return editors_.value(mainEditorId_, nullptr);
}

// ============================================================================
// UI Contributions
// ============================================================================

QList<NativeViewContribution> NodeEditorPlugin::native_view_contributions() const {
    return registeredContributions_;
}

QWidget *NodeEditorPlugin::getNativeWidget(const QString &viewId) {
    if (auto *instance = editors_.value(viewId, nullptr)) {
        return instance->widget.data();// QPointer::data() 返回原始指针
    }
    return nullptr;
}

QObject *NodeEditorPlugin::getViewModel(const QString &viewId) {
    if (auto *instance = editors_.value(viewId, nullptr)) {
        return instance->viewModel;
    }
    return nullptr;
}

void NodeEditorPlugin::register_view_models(QQmlEngine *engine) {
    if (!engine) {
        qWarning() << "NodeEditorPlugin::register_view_models: engine is null";
        return;
    }

    // Register NodeEditorViewModel as QML type (for potential QML overlays)
    qmlRegisterType<NodeEditorViewModel>("RoboCute.NodeEditor", 1, 0, "NodeEditorViewModel");

    qDebug() << "NodeEditorPlugin: ViewModels registered";
}

void NodeEditorPlugin::buildMenuContributions() {
    menuContributions_.clear();

    // Node Editor menu
    MenuContribution newGraph;
    newGraph.menuPath = "Graph";
    newGraph.actionText = "New Graph";
    newGraph.actionId = "node_editor.new_graph";
    newGraph.shortcut = "Ctrl+N";
    newGraph.callback = [this]() {
        if (auto *editor = mainNodeEditor()) {
            if (editor->viewModel) {
                editor->viewModel->newGraph();
            }
        }
    };
    menuContributions_.append(newGraph);

    MenuContribution saveGraph;
    saveGraph.menuPath = "Graph";
    saveGraph.actionText = "Save Graph";
    saveGraph.actionId = "node_editor.save_graph";
    saveGraph.shortcut = "Ctrl+S";
    saveGraph.callback = [this]() {
        if (auto *editor = mainNodeEditor()) {
            if (editor->viewModel) {
                editor->viewModel->saveGraph();
            }
        }
    };
    menuContributions_.append(saveGraph);

    MenuContribution loadGraph;
    loadGraph.menuPath = "Graph";
    loadGraph.actionText = "Load Graph";
    loadGraph.actionId = "node_editor.load_graph";
    loadGraph.shortcut = "Ctrl+O";
    loadGraph.callback = [this]() {
        if (auto *editor = mainNodeEditor()) {
            if (editor->viewModel) {
                editor->viewModel->loadGraph();
            }
        }
    };
    menuContributions_.append(loadGraph);

    MenuContribution executeGraph;
    executeGraph.menuPath = "Graph";
    executeGraph.actionText = "Execute Graph";
    executeGraph.actionId = "node_editor.execute_graph";
    executeGraph.shortcut = "Ctrl+Enter";
    executeGraph.callback = [this]() {
        if (auto *editor = mainNodeEditor()) {
            if (editor->viewModel) {
                editor->viewModel->executeGraph();
            }
        }
    };
    menuContributions_.append(executeGraph);

    MenuContribution refreshNodes;
    refreshNodes.menuPath = "Graph";
    refreshNodes.actionText = "Refresh Nodes";
    refreshNodes.actionId = "node_editor.refresh_nodes";
    refreshNodes.shortcut = "";
    refreshNodes.callback = [this]() {
        if (auto *editor = mainNodeEditor()) {
            if (editor->viewModel) {
                editor->viewModel->refreshNodes();
            }
        }
    };
    menuContributions_.append(refreshNodes);
}

QList<MenuContribution> NodeEditorPlugin::menu_contributions() const {
    return menuContributions_;
}

// ============================================================================
// Factory Export
// ============================================================================

// Export factory function for dynamic loading
// PluginManager uses the factory to manage plugin lifecycle
IPluginFactory *createPluginFactory() {
    return new PluginFactory<NodeEditorPlugin>();
}

}// namespace rbc
