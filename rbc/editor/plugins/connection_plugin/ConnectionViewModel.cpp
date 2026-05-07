#include "ConnectionPlugin.h"
#include <QDebug>
#include <QTimer>

namespace rbc {

ConnectionViewModel::ConnectionViewModel(ConnectionService *connectionService, QObject *parent)
    : ViewModelBase(parent), _connection_service(connectionService) {

    if (!_connection_service) {
        qWarning() << "ConnectionViewModel: connectionService is null";
        return;
    }

    // Connect to service signals
    QObject::connect(_connection_service, &ConnectionService::serverUrlChanged,
                     this, &ConnectionViewModel::serverUrlChanged);
    QObject::connect(_connection_service, &ConnectionService::connectedChanged,
                     this, &ConnectionViewModel::onConnectionStatusChanged);
    QObject::connect(_connection_service, &ConnectionService::statusTextChanged,
                     this, &ConnectionViewModel::onStatusTextChanged);
    QObject::connect(_connection_service, &ConnectionService::connectionTested,
                     this, &ConnectionViewModel::onConnectionTested);
}

ConnectionViewModel::~ConnectionViewModel() {
    // Service lifetime is longer than ViewModel, explicitly disconnect signal-slot connections with Service during destruction
    if (_connection_service) {
        // Disconnect all connections from _connection_service to this
        QObject::disconnect(_connection_service, nullptr, this, nullptr);
    }
    _connection_service = nullptr;
}

QString ConnectionViewModel::serverUrl() const {
    return _connection_service ? _connection_service->serverUrl() : QString();
}

void ConnectionViewModel::setServerUrl(const QString &url) {
    if (_connection_service) {
        _connection_service->setServerUrl(url);
    }
}

bool ConnectionViewModel::connected() const {
    return _connection_service ? _connection_service->connected() : false;
}

QString ConnectionViewModel::statusText() const {
    return _connection_service ? _connection_service->statusText() : QString("Disconnected");
}

void ConnectionViewModel::testConnection() {
    if (_connection_service) {
        setBusy(true);
        _connection_service->testConnection();
        // Busy state will be reset when connectionTested signal is received
    }
}

void ConnectionViewModel::connect() {
    if (_connection_service) {
        setBusy(true);
        _connection_service->connect();
    }
}

void ConnectionViewModel::disconnect() {
    if (_connection_service) {
        _connection_service->disconnect();
        setBusy(false);
    }
}

void ConnectionViewModel::onConnectionStatusChanged() {
    emit connectedChanged();
    setBusy(false);
}

void ConnectionViewModel::onStatusTextChanged() {
    emit statusTextChanged();
}

void ConnectionViewModel::onConnectionTested(bool success) {
    setBusy(false);
    // The status will be updated via onConnectionStatusChanged and onStatusTextChanged
}

}// namespace rbc
