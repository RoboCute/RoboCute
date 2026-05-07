#include "ConnectionPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"

#include <QQmlEngine>
#include <QDebug>
#include <QTimer>

namespace rbc {

ConnectionPlugin::ConnectionPlugin(QObject *parent)
    : IEditorPlugin(parent) {
    qDebug() << "ConnectionPlugin created" << STR(RBCE_PLUGIN_PATH);
}

ConnectionPlugin::~ConnectionPlugin() {
    qDebug() << "ConnectionPlugin destroyed";
}

bool ConnectionPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ConnectionPlugin::load: context is null";
        return false;
    }

    _context = context;

    // Get ConnectionService from PluginManager
    _connection_service = context->getService<ConnectionService>();

    if (!_connection_service) {
        qWarning() << "ConnectionPlugin::load: ConnectionService should be registered before ConnectionPlugin load";
        return false;
    }

    // Create ViewModel
    _view_model = new ConnectionViewModel(_connection_service, this);

    qDebug() << "ConnectionPlugin loaded successfully";
    // qDebug() << "ConnectionPlugin loaded at " << STR(RBCE_PLUGIN_PATH);
    return true;
}

bool ConnectionPlugin::unload() {
    if (_view_model) {
        _view_model->deleteLater();
        _view_model = nullptr;
    }

    // Don't delete _connection_service as it might be used by other plugins
    _connection_service = nullptr;
    _context = nullptr;

    qDebug() << "ConnectionPlugin unloaded";
    return true;
}

bool ConnectionPlugin::reload() {
    qDebug() << "ConnectionPlugin reloading...";

    // Save current state if needed
    QString savedUrl = _connection_service ? _connection_service->serverUrl() : QString();
    bool wasConnected = _connection_service ? _connection_service->connected() : false;

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(_context)) {
        return false;
    }

    // Restore state
    if (_connection_service && !savedUrl.isEmpty()) {
        _connection_service->setServerUrl(savedUrl);
        if (wasConnected) {
            _connection_service->connect();
        }
    }

    qDebug() << "ConnectionPlugin reloaded";
    return true;
}

QList<ViewContribution> ConnectionPlugin::view_contributions() const {
    ViewContribution view;
    view.viewId = "connection_status";
    view.title = "Connection Status";
    // qml file is compiled into the plugin's qrc under qrc:/qml/
    // keep the relative path minimal so WindowManager resolves it correctly
    view.qmlSource = "ConnectionView.qml";
    view.qmlHotDir = STR(RBCE_PLUGIN_PATH);
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

QObject *ConnectionPlugin::getViewModel(const QString &viewId) {
    if (viewId == "connection_status" && _view_model) {
        return _view_model;
    }
    return nullptr;
}

// Export factory function
// PluginManager manages plugin lifecycles through factories
IPluginFactory *createPluginFactory() {
    return new PluginFactory<ConnectionPlugin>();
}

}// namespace rbc
