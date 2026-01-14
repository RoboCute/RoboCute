#include "ConnectionPlugin.h"
#include <QDebug>
#include <QTimer>

namespace rbc {

ConnectionViewModel::ConnectionViewModel(ConnectionService *connectionService, QObject *parent)
    : ViewModelBase(parent), connectionService_(connectionService) {

    if (!connectionService_) {
        qWarning() << "ConnectionViewModel: connectionService is null";
        return;
    }

    // Connect to service signals
    QObject::connect(connectionService_, &ConnectionService::serverUrlChanged,
            this, &ConnectionViewModel::serverUrlChanged);
    QObject::connect(connectionService_, &ConnectionService::connectedChanged,
            this, &ConnectionViewModel::onConnectionStatusChanged);
    QObject::connect(connectionService_, &ConnectionService::statusTextChanged,
            this, &ConnectionViewModel::onStatusTextChanged);
    QObject::connect(connectionService_, &ConnectionService::connectionTested,
            this, &ConnectionViewModel::onConnectionTested);
}

ConnectionViewModel::~ConnectionViewModel() {
    // 关键：在析构时显式断开与 Service 的信号槽连接
    // 原因同 ProjectViewModel：Service 生命周期比 ViewModel 长
    if (connectionService_) {
        // 断开所有从 connectionService_ 到 this 的连接
        QObject::disconnect(connectionService_, nullptr, this, nullptr);
    }
    connectionService_ = nullptr;
}

QString ConnectionViewModel::serverUrl() const {
    return connectionService_ ? connectionService_->serverUrl() : QString();
}

void ConnectionViewModel::setServerUrl(const QString &url) {
    if (connectionService_) {
        connectionService_->setServerUrl(url);
    }
}

bool ConnectionViewModel::connected() const {
    return connectionService_ ? connectionService_->connected() : false;
}

QString ConnectionViewModel::statusText() const {
    return connectionService_ ? connectionService_->statusText() : QString("Disconnected");
}

void ConnectionViewModel::testConnection() {
    if (connectionService_) {
        setBusy(true);
        connectionService_->testConnection();
        // Busy state will be reset when connectionTested signal is received
    }
}

void ConnectionViewModel::connect() {
    if (connectionService_) {
        setBusy(true);
        connectionService_->connect();
    }
}

void ConnectionViewModel::disconnect() {
    if (connectionService_) {
        connectionService_->disconnect();
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
