#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/services/ConnectionService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

#ifndef RBCE_PLUGIN_PATH
#define RBCE_PLUGIN_PATH ""
#endif
#define _STR(x) #x
#define STR(x) _STR(x)

namespace rbc {

/**
 * ConnectionViewModel - Connection status view model
 * 
 * Manages connection status display and interaction logic
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
    [[nodiscard]] ConnectionService *connectionService() const { return _connection_service; }

signals:
    void serverUrlChanged();
    void connectedChanged();
    void statusTextChanged();

private slots:
    void onConnectionStatusChanged();
    void onStatusTextChanged();
    void onConnectionTested(bool success);

private:
    ConnectionService *_connection_service = nullptr;
};

/**
 * ConnectionPlugin - Connection status plugin
 * 
 * Provides a UI panel for connection status management
 */
class RBC_EDITOR_PLUGIN_API ConnectionPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit ConnectionPlugin(QObject *parent = nullptr);
    ~ConnectionPlugin() override;

    // === Static Methods for Factory ===
    static QString staticPluginId() { return "com.robocute.connection"; }
    static QString staticPluginName() { return "Connection Plugin"; }

    // IEditorPlugin interface
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }

    QList<ViewContribution> view_contributions() const override;
    QList<MenuContribution> menu_contributions() const override { return {}; }
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }

    void register_view_models(QQmlEngine *engine) override;

    // Get ViewModel for a specific view
    QObject *getViewModel(const QString &viewId) override;

private:
    ConnectionService *_connection_service = nullptr;
    ConnectionViewModel *_view_model = nullptr;
    PluginContext *_context = nullptr;
};

// Export factory function
class IPluginFactory;
LUISA_EXPORT_API IPluginFactory *createPluginFactory();

}// namespace rbc
