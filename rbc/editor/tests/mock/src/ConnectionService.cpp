#include "RBCEditorMock/ConnectionService.h"
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>

namespace rbc {

MockConnectionService::MockConnectionService(QObject *parent)
    : IConnectionService(parent), _delay_timer(new QTimer(this)) {
    _delay_timer->setSingleShot(true);
    QObject::connect(_delay_timer, &QTimer::timeout, this, &MockConnectionService::onDelayedConnection);
}

QString MockConnectionService::serverUrl() const {
    return "mock.server.url";
}

bool MockConnectionService::connected() const {
    // Return mock state (this will be called when using MockConnectionService*)
    return _mock_connected;
}

QString MockConnectionService::statusText() const {
    // Return mock state (this will be called when using MockConnectionService*)
    return _mock_status_text;
}

void MockConnectionService::testConnection() {
    qDebug() << "[MockConnectionService] testConnection called";
    // Default behavior: simulate success after a short delay
    if (_auto_connect_delay > 0) {
        QTimer::singleShot(static_cast<int>(_auto_connect_delay), this, [this]() {
            simulateTestSuccess();
        });
    } else {
        simulateTestSuccess();
    }
}

void MockConnectionService::connect() {
    qDebug() << "[MockConnectionService] connect called for" << serverUrl();

    if (serverUrl().isEmpty()) {
        _mock_connected = false;
        _mock_status_text = "No server URL set";
        emit connectedChanged();
        emit statusTextChanged();
        return;
    }

    // Set connecting status
    _mock_connected = false;
    _mock_status_text = "Connecting ...";
    emit statusTextChanged();

    // Simulate the connection behavior
    if (_auto_connect_delay > 0) {
        _delay_timer->start(_auto_connect_delay);
    } else {
        onDelayedConnection();
    }
}

void MockConnectionService::disconnect() {
    qDebug() << "[MockConnectionService] disconnect called";
    _delay_timer->stop();
    _mock_connected = false;
    _mock_status_text = "Disconnected";
    emit connectedChanged();
    emit statusTextChanged();
}

void MockConnectionService::simulateConnectionSuccess() {
    qDebug() << "[MockConnectionService] Simulating connection success";
    _mock_connected = true;
    _mock_status_text = "Connected";
    emit connectedChanged();
    emit statusTextChanged();
    emit connectionTested(true);
}

void MockConnectionService::simulateConnectionFailure(const QString &errorMessage) {
    qDebug() << "[MockConnectionService] Simulating connection failure:" << errorMessage;
    _mock_connected = false;
    _mock_status_text = errorMessage;
    emit connectedChanged();
    emit statusTextChanged();
    emit connectionTested(false);
}

void MockConnectionService::simulateTestSuccess() {
    qDebug() << "[MockConnectionService] Simulating test success";
    emit connectionTested(true);
    // Don't change connection status for test
}

void MockConnectionService::simulateTestFailure() {
    qDebug() << "[MockConnectionService] Simulating test failure";
    emit connectionTested(false);
}

void MockConnectionService::setAutoConnectDelay(int milliseconds) {
    _auto_connect_delay = milliseconds;
}

void MockConnectionService::reset() {
    _delay_timer->stop();
    setServerUrl("http://127.0.0.1:5555");
    _mock_connected = false;
    _mock_status_text = "Disconnected";
    _auto_connect_delay = 0;
    emit connectedChanged();
    emit statusTextChanged();
}

void MockConnectionService::onDelayedConnection() {
    // Default: simulate success
    simulateConnectionSuccess();
}

}// namespace rbc
