#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/services/ConnectionService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

namespace rbc {

/**
 * ConnectionViewModel - 连接状态视图模型
 * 
 * 负责管理连接状态的显示和交互逻辑
 */
class RBC_EDITOR_PLUGIN_API ConnectionViewModel : public ViewModelBase {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit ConnectionViewModel(ConnectionService *connectionService, QObject *parent = nullptr);
    ~ConnectionViewModel() override;

    // Property accessors
    QString serverUrl() const;
    void setServerUrl(const QString &url);
    bool connected() const;
    QString statusText() const;

    // QML invokable methods
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void connect();
    Q_INVOKABLE void disconnect();

    // Service access
    ConnectionService *connectionService() const { return connectionService_; }

signals:
    void serverUrlChanged();
    void connectedChanged();
    void statusTextChanged();

private slots:
    void onConnectionStatusChanged();
    void onStatusTextChanged();
    void onConnectionTested(bool success);

private:
    ConnectionService *connectionService_ = nullptr;
};

/**
 * ConnectionPlugin - 连接状态插件
 * 
 * 提供连接状态管理的 UI 面板
 */
class RBC_EDITOR_PLUGIN_API ConnectionPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit ConnectionPlugin(QObject *parent = nullptr);
    ~ConnectionPlugin() override;

    // IEditorPlugin interface
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return "com.robocute.connection"; }
    QString name() const override { return "Connection Plugin"; }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }
    bool is_dynamic() const override { return true; }

    QList<ViewContribution> view_contributions() const override;
    QList<MenuContribution> menu_contributions() const override { return {}; }
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }

    void register_view_models(QQmlEngine *engine) override;

    // Get ViewModel for a specific view
    QObject *getViewModel(const QString &viewId) override;

private:
    ConnectionService *connectionService_ = nullptr;
    ConnectionViewModel *viewModel_ = nullptr;
    PluginContext *context_ = nullptr;
};

LUISA_EXPORT_API IEditorPlugin *createPlugin();

}// namespace rbc
