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
    : ViewModelBase(parent), _config(config), _editor(editor), _server_url(config.serverUrl) {

    if (!_editor) {
        qWarning() << "NodeEditorViewModel: editor is null for:" << config.editorId;
    }

    if (_editor) {
        _editor->loadNodesDeferred();
    }
}

NodeEditorViewModel::~NodeEditorViewModel() {
    _editor = nullptr;
}

void NodeEditorViewModel::setServerUrl(const QString &url) {
    if (_server_url != url) {
        _server_url = url;
        emit serverUrlChanged();
        qDebug() << "NodeEditorViewModel:" << _config.editorId << "server URL changed:" << url;
    }
}

void NodeEditorViewModel::connectToServer() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "connectToServer";
    if (_editor) {
        _editor->loadNodesDeferred();
    }
}

void NodeEditorViewModel::disconnectFromServer() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "disconnectFromServer";
    // TODO: Implement disconnect logic
    _connected = false;
    _status_text = "Disconnected";
    emit connectedChanged();
    emit statusTextChanged();
}

void NodeEditorViewModel::executeGraph() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "executeGraph";
    // The NodeEditor handles execution internally
    // We can trigger it via a slot if needed
}

void NodeEditorViewModel::refreshNodes() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "refreshNodes";
    if (_editor) {
        _editor->loadNodesDeferred();
    }
}

void NodeEditorViewModel::newGraph() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "newGraph";
    // TODO: Trigger new graph action on editor
}

void NodeEditorViewModel::saveGraph() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "saveGraph";
    // TODO: Trigger save graph action on editor
}

void NodeEditorViewModel::loadGraph() {
    qDebug() << "NodeEditorViewModel:" << _config.editorId << "loadGraph";
    // TODO: Trigger load graph action on editor
}

void NodeEditorViewModel::onConnectionStatusChanged(bool connected) {
    if (_connected != connected) {
        _connected = connected;
        _status_text = connected ? "Connected" : "Disconnected";
        emit connectedChanged();
        emit statusTextChanged();
    }
}

void NodeEditorViewModel::onExecutionStateChanged(bool executing) {
    if (_executing != executing) {
        _executing = executing;
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

    _context = context;

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
    _registered_contributions.clear();
    _menu_contributions.clear();
    _context = nullptr;
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

    _main_editor_id = createNodeEditor(mainConfig);

    if (!_main_editor_id.isEmpty()) {
        // Register to contributions
        NativeViewContribution mainContrib;
        mainContrib.viewId = mainConfig.editorId;
        mainContrib.title = "Node Editor";
        mainContrib.dockArea = "Bottom";     // Default dock area, layout will override
        mainContrib.isExternalManaged = true;// Plugin manages widget lifecycle
        mainContrib.closable = true;
        mainContrib.movable = true;
        mainContrib.floatable = true;

        _registered_contributions.append(mainContrib);

        qDebug() << "NodeEditorPlugin: Created main node editor:" << _main_editor_id;
    }
}

QString NodeEditorPlugin::createNodeEditor(const NodeEditorConfig &config) {
    if (_editors.contains(config.editorId)) {
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

    _editors.insert(config.editorId, instance);

    emit nodeEditorCreated(config.editorId);
    qDebug() << "NodeEditorPlugin: Created node editor:" << config.editorId;

    return config.editorId;
}

bool NodeEditorPlugin::destroyNodeEditor(const QString &editorId) {
    auto it = _editors.find(editorId);
    if (it == _editors.end()) {
        qWarning() << "NodeEditorPlugin::destroyNodeEditor: Editor not found:" << editorId;
        return false;
    }

    NodeEditorInstance *instance = it.value();

    // Delete Widget
    // Use QPointer to check if widget still exists
    // If Qt already deleted widget (e.g., via parent-child mechanism), QPointer becomes nullptr
    if (instance->widget) {
        qDebug() << "NodeEditorPlugin::destroyNodeEditor: Deleting widget for:" << editorId;
        delete instance->widget.data();
        // QPointer auto-becomes nullptr, no manual setting needed
    } else {
        qDebug() << "NodeEditorPlugin::destroyNodeEditor: Widget already deleted for:" << editorId;
    }

    // Delete instance (destructor cleans up viewModel)
    delete instance;

    _editors.erase(it);

    // Clear main reference if this was the main editor
    if (editorId == _main_editor_id) {
        _main_editor_id.clear();
    }

    // Remove from contributions
    _registered_contributions.erase(
        std::remove_if(
            _registered_contributions.begin(), _registered_contributions.end(),
            [&editorId](const NativeViewContribution &c) {
                return c.viewId == editorId;
            }),
        _registered_contributions.end());

    emit nodeEditorDestroyed(editorId);
    qDebug() << "NodeEditorPlugin: Destroyed node editor:" << editorId;

    return true;
}

void NodeEditorPlugin::destroyAllNodeEditors() {
    QStringList ids = _editors.keys();
    for (const QString &id : ids) {
        destroyNodeEditor(id);
    }
}

NodeEditorInstance *NodeEditorPlugin::getNodeEditor(const QString &editorId) {
    return _editors.value(editorId, nullptr);
}

QStringList NodeEditorPlugin::allEditorIds() const {
    return _editors.keys();
}

NodeEditorInstance *NodeEditorPlugin::mainNodeEditor() const {
    if (_main_editor_id.isEmpty()) {
        return nullptr;
    }
    return _editors.value(_main_editor_id, nullptr);
}

// ============================================================================
// UI Contributions
// ============================================================================

QList<NativeViewContribution> NodeEditorPlugin::native_view_contributions() const {
    return _registered_contributions;
}

QWidget *NodeEditorPlugin::getNativeWidget(const QString &viewId) {
    if (auto *instance = _editors.value(viewId, nullptr)) {
        return instance->widget.data();// QPointer::data() returns raw pointer
    }
    return nullptr;
}

QObject *NodeEditorPlugin::getViewModel(const QString &viewId) {
    if (auto *instance = _editors.value(viewId, nullptr)) {
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
    _menu_contributions.clear();

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
    _menu_contributions.append(newGraph);

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
    _menu_contributions.append(saveGraph);

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
    _menu_contributions.append(loadGraph);

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
    _menu_contributions.append(executeGraph);

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
    _menu_contributions.append(refreshNodes);
}

QList<MenuContribution> NodeEditorPlugin::menu_contributions() const {
    return _menu_contributions;
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
