#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include <memory>

namespace rbc {

// Forward declarations
class NodeEditor;
class HttpClient;
struct PluginContext;

// ============================================================================
// NodeEditorConfig - 节点编辑器配置
// ============================================================================

struct NodeEditorConfig {
    QString editorId;          // uid for this node editor
    QString serverUrl;         // backend server URL
    bool autoConnect = true;   // auto connect on load
};

// ============================================================================
// NodeEditorViewModel - 节点编辑器视图模型
// ============================================================================

/**
 * @brief NodeEditorViewModel - 节点编辑器状态视图模型
 * 
 * 管理节点编辑器的状态：
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
    QString editorId() const { return config_.editorId; }
    QString serverUrl() const { return serverUrl_; }
    void setServerUrl(const QString &url);
    bool connected() const { return connected_; }
    bool executing() const { return executing_; }
    QString statusText() const { return statusText_; }

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
    NodeEditorConfig config_;
    NodeEditor *editor_ = nullptr;
    QString serverUrl_;
    bool connected_ = false;
    bool executing_ = false;
    QString statusText_ = "Disconnected";
};

// ============================================================================
// NodeEditorInstance - 节点编辑器实例
// ============================================================================

/**
 * @brief NodeEditorInstance - 将 Widget 和 ViewModel 组合在一起
 */
struct NodeEditorInstance {
    NodeEditorConfig config;
    NodeEditor *widget = nullptr;
    NodeEditorViewModel *viewModel = nullptr;

    ~NodeEditorInstance() {
        delete viewModel;
        viewModel = nullptr;
        // widget 生命周期由 Qt parent-child 机制管理
    }
};

// ============================================================================
// NodeEditorPlugin - 节点编辑器管理插件
// ============================================================================

/**
 * @brief NodeEditorPlugin - 节点编辑器管理插件
 * 
 * 职责：
 * 1. 管理 NodeEditor 的生命周期
 * 2. 提供节点编辑器创建/销毁 API
 * 3. 通过 NativeViewContribution 向 WindowManager 注册节点编辑器
 * 4. 协调 NodeEditorViewModel 与后端服务的交互
 * 
 * 布局配置：
 * - scene_editing: 放在 Bottom dock
 * - aigc: 放在 Center
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
     * @brief 创建新的节点编辑器
     * @param config 编辑器配置
     * @return 编辑器 ID，失败返回空字符串
     */
    QString createNodeEditor(const NodeEditorConfig &config);

    /**
     * @brief 销毁节点编辑器
     * @param editorId 编辑器 ID
     * @return 是否成功
     */
    bool destroyNodeEditor(const QString &editorId);

    /**
     * @brief 获取节点编辑器实例
     * @param editorId 编辑器 ID
     * @return 编辑器实例，未找到返回 nullptr
     */
    NodeEditorInstance *getNodeEditor(const QString &editorId);

    /**
     * @brief 获取所有节点编辑器 ID
     */
    QStringList allEditorIds() const;

    /**
     * @brief 获取主节点编辑器实例
     */
    NodeEditorInstance *mainNodeEditor() const;

signals:
    void nodeEditorCreated(const QString &editorId);
    void nodeEditorDestroyed(const QString &editorId);

private:
    void createDefaultNodeEditor();
    void destroyAllNodeEditors();
    void buildMenuContributions();

    PluginContext *context_ = nullptr;
    HttpClient *httpClient_ = nullptr;

    // 节点编辑器实例管理
    QHash<QString, NodeEditorInstance *> editors_;
    QString mainEditorId_;

    // 预注册的 Native View Contributions
    QList<NativeViewContribution> registeredContributions_;
    QList<MenuContribution> menuContributions_;
};

// 导出工厂函数
class IPluginFactory;
LUISA_EXPORT_API IPluginFactory *createPluginFactory();

}// namespace rbc
