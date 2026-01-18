#include "MockConnectionService.h"
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>

namespace rbc {

MockConnectionService::MockConnectionService(QObject *parent)
    : IConnectionService(parent), m_delayTimer(new QTimer(this)) {
    m_delayTimer->setSingleShot(true);
    QObject::connect(m_delayTimer, &QTimer::timeout, this, &MockConnectionService::onDelayedConnection);
}

QString MockConnectionService::serverUrl() const {
    return "mock.server.url";
}

bool MockConnectionService::connected() const {
    // Return mock state (this will be called when using MockConnectionService*)
    return m_mockConnected;
}

QString MockConnectionService::statusText() const {
    // Return mock state (this will be called when using MockConnectionService*)
    return m_mockStatusText;
}

void MockConnectionService::testConnection() {
    qDebug() << "[MockConnectionService] testConnection called";
    // Default behavior: simulate success after a short delay
    if (m_autoConnectDelay > 0) {
        QTimer::singleShot(static_cast<int>(m_autoConnectDelay), this, [this]() {
            simulateTestSuccess();
        });
    } else {
        simulateTestSuccess();
    }
}

void MockConnectionService::connect() {
    qDebug() << "[MockConnectionService] connect called for" << serverUrl();

    if (serverUrl().isEmpty()) {
        m_mockConnected = false;
        m_mockStatusText = "No server URL set";
        emit connectedChanged();
        emit statusTextChanged();
        return;
    }

    // Set connecting status
    m_mockConnected = false;
    m_mockStatusText = "Connecting ...";
    emit statusTextChanged();

    // Simulate the connection behavior
    if (m_autoConnectDelay > 0) {
        m_delayTimer->start(m_autoConnectDelay);
    } else {
        onDelayedConnection();
    }
}

void MockConnectionService::disconnect() {
    qDebug() << "[MockConnectionService] disconnect called";
    m_delayTimer->stop();
    m_mockConnected = false;
    m_mockStatusText = "Disconnected";
    emit connectedChanged();
    emit statusTextChanged();
}

void MockConnectionService::simulateConnectionSuccess() {
    qDebug() << "[MockConnectionService] Simulating connection success";
    m_mockConnected = true;
    m_mockStatusText = "Connected";
    emit connectedChanged();
    emit statusTextChanged();
    emit connectionTested(true);
}

void MockConnectionService::simulateConnectionFailure(const QString &errorMessage) {
    qDebug() << "[MockConnectionService] Simulating connection failure:" << errorMessage;
    m_mockConnected = false;
    m_mockStatusText = errorMessage;
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
    m_autoConnectDelay = milliseconds;
}

void MockConnectionService::reset() {
    m_delayTimer->stop();
    setServerUrl("http://127.0.0.1:5555");
    m_mockConnected = false;
    m_mockStatusText = "Disconnected";
    m_autoConnectDelay = 0;
    emit connectedChanged();
    emit statusTextChanged();
}

void MockConnectionService::onDelayedConnection() {
    // Default: simulate success
    simulateConnectionSuccess();
}

}// namespace rbc
