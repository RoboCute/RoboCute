#include "ConnectionPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include <QQmlEngine>
#include <QDebug>
#include <QTimer>

namespace rbc {

ConnectionPlugin::ConnectionPlugin(QObject *parent)
    : IEditorPlugin(parent) {
}

bool ConnectionPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ConnectionPlugin::load: context is null";
        return false;
    }

    context_ = context;

    // Get ConnectionService from PluginManager
    auto &pm = EditorPluginManager::instance();
    connectionService_ = pm.getService<ConnectionService>();
    
    if (!connectionService_) {
        qWarning() << "ConnectionPlugin::load: ConnectionService not found, creating default";
        // Create a default ConnectionService if not registered
        connectionService_ = new ConnectionService(this);
        pm.registerService("ConnectionService", connectionService_);
    }

    // Create ViewModel
    viewModel_ = new ConnectionViewModel(connectionService_, this);

    qDebug() << "ConnectionPlugin loaded successfully";
    return true;
}

bool ConnectionPlugin::unload() {
    if (viewModel_) {
        viewModel_->deleteLater();
        viewModel_ = nullptr;
    }

    // Don't delete connectionService_ as it might be used by other plugins
    connectionService_ = nullptr;
    context_ = nullptr;

    qDebug() << "ConnectionPlugin unloaded";
    return true;
}

bool ConnectionPlugin::reload() {
    qDebug() << "ConnectionPlugin reloading...";
    
    // Save current state if needed
    QString savedUrl = connectionService_ ? connectionService_->serverUrl() : QString();
    bool wasConnected = connectionService_ ? connectionService_->connected() : false;

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(context_)) {
        return false;
    }

    // Restore state
    if (connectionService_ && !savedUrl.isEmpty()) {
        connectionService_->setServerUrl(savedUrl);
        if (wasConnected) {
            connectionService_->connect();
        }
    }

    qDebug() << "ConnectionPlugin reloaded";
    return true;
}

QList<ViewContribution> ConnectionPlugin::view_contributions() const {
    ViewContribution view;
    view.viewId = "connection_status";
    view.title = "Connection Status";
    view.qmlSource = "connection_plugin/ConnectionView.qml";
    view.dockArea = "Left";
    view.preferredSize = "300,200";
    view.closable = true;
    view.movable = true;
    
    return {view};
}

void ConnectionPlugin::register_view_models(QQmlEngine *engine) {
    if (!engine) {
        qWarning() << "ConnectionPlugin::register_view_models: engine is null";
        return;
    }

    // Register ConnectionViewModel as QML type
    qmlRegisterType<ConnectionViewModel>("RoboCute.Connection", 1, 0, "ConnectionViewModel");
    
    qDebug() << "ConnectionPlugin: ViewModels registered";
}

}// namespace rbc

