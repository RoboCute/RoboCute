#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/ui/NodeEditor.h"
#include <QPointer>

namespace rbc {

// ============================================================================
// NodeEditorConfig - Node editor configuration
// ============================================================================

struct NodeEditorConfig {
    QString editorId;       // uid for this node editor
    QString serverUrl;      // backend server URL
    bool autoConnect = true;// auto connect on load
};

// ============================================================================
// NodeEditorViewModel - Node editor view model
// ============================================================================

/**
 * @brief NodeEditorViewModel - Node editor state view model
 * 
 * Manages node editor state:
 * - Server connection state
 * - Graph execution state
 * - Node types / categories
 */
class RBC_EDITOR_PLUGIN_API NodeEditorViewModel : public ViewModelBase {
    Q_OBJECT
    Q_PROPERTY(QString editorId READ editorId CONSTANT)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool executing READ executing NOTIFY executingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit NodeEditorViewModel(
        const NodeEditorConfig &config,
        NodeEditor *editor,
        QObject *parent = nullptr);
    ~NodeEditorViewModel() override;

    // Properties
    QString editorId() const { return _config.editorId; }
    QString serverUrl() const { return _server_url; }
    void setServerUrl(const QString &url);
    bool connected() const { return _connected; }
    bool executing() const { return _executing; }
    QString statusText() const { return _status_text; }

    // QML invokable methods
    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void executeGraph();
    Q_INVOKABLE void refreshNodes();
    Q_INVOKABLE void newGraph();
    Q_INVOKABLE void saveGraph();
    Q_INVOKABLE void loadGraph();

signals:
    void serverUrlChanged();
    void connectedChanged();
    void executingChanged();
    void statusTextChanged();

public slots:
    void onConnectionStatusChanged(bool connected);
    void onExecutionStateChanged(bool executing);

private:
    NodeEditorConfig _config;
    NodeEditor *_editor = nullptr;
    QString _server_url;
    bool _connected = false;
    bool _executing = false;
    QString _status_text = "Disconnected";
};

// ============================================================================
// NodeEditorInstance - Node editor instance
// ============================================================================

/**
 * @brief NodeEditorInstance - Combines Widget and ViewModel
 */
struct NodeEditorInstance {
    NodeEditorConfig config;
    QPointer<NodeEditor> widget;// Tracked with QPointer, auto-detects deletion
    NodeEditorViewModel *viewModel = nullptr;

    ~NodeEditorInstance() {
        delete viewModel;
        viewModel = nullptr;
        // widget uses QPointer, not deleted here
        // Explicitly managed by destroyNodeEditor or Qt parent-child mechanism
    }
};

// ============================================================================
// NodeEditorPlugin - Node editor management plugin
// ============================================================================

/**
 * @brief NodeEditorPlugin - Node editor management plugin
 * 
 * Responsibilities:
 * 1. Manage NodeEditor lifecycle
 * 2. Provide node editor create/destroy API
 * 3. Register node editors with WindowManager via NativeViewContribution
 * 4. Coordinate NodeEditorViewModel and backend service interaction
 * 
 * Layout config:
 * - scene_editing: placed in Bottom dock
 * - aigc: placed in Center
 */
class RBC_EDITOR_PLUGIN_API NodeEditorPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit NodeEditorPlugin(QObject *parent = nullptr);
    ~NodeEditorPlugin() override;

    // === Static Methods for Factory ===
    static QString staticPluginId() { return "com.robocute.node_editor"; }
    static QString staticPluginName() { return "Node Editor Plugin"; }

    // === IEditorPlugin Interface ===
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }

    // UI Contributions
    QList<ViewContribution> view_contributions() const override { return {}; }
    QList<MenuContribution> menu_contributions() const override;
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }
    QList<NativeViewContribution> native_view_contributions() const override;

    void register_view_models(QQmlEngine *engine) override;
    QObject *getViewModel(const QString &viewId) override;
    QWidget *getNativeWidget(const QString &viewId) override;

    // === Node Editor Management API ===

    /**
     * @brief Create a new node editor
     * @param config Editor configuration
     * @return Editor ID, empty string on failure
     */
    QString createNodeEditor(const NodeEditorConfig &config);

    /**
     * @brief Destroy a node editor
     * @param editorId Editor ID
     * @return Whether successful
     */
    bool destroyNodeEditor(const QString &editorId);

    /**
     * @brief Get node editor instance
     * @param editorId Editor ID
     * @return Editor instance, nullptr if not found
     */
    NodeEditorInstance *getNodeEditor(const QString &editorId);

    /**
     * @brief Get all node editor IDs
     */
    QStringList allEditorIds() const;

    /**
     * @brief Get main node editor instance
     */
    NodeEditorInstance *mainNodeEditor() const;

signals:
    void nodeEditorCreated(const QString &editorId);
    void nodeEditorDestroyed(const QString &editorId);

private:
    void createDefaultNodeEditor();
    void destroyAllNodeEditors();
    void buildMenuContributions();

    PluginContext *_context = nullptr;
    // Node editor instance management
    QHash<QString, NodeEditorInstance *> _editors;
    QString _main_editor_id;

    QList<NativeViewContribution> _registered_contributions;
    QList<MenuContribution> _menu_contributions;
};

// Export factory function
class IPluginFactory;
LUISA_EXPORT_API IPluginFactory *createPluginFactory();

}// namespace rbc
