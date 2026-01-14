#include "RBCEditorRuntime/services/ConnectionService.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>

namespace rbc {

ConnectionService::ConnectionService(QObject *parent) : IService(parent) {
    m_healthCheckTimer = new QTimer(this);
    m_healthCheckTimer->setInterval(5000);
    QObject::connect(m_healthCheckTimer, &QTimer::timeout, this, &ConnectionService::performHealthCheck);
}

ConnectionService::~ConnectionService() {
    // Stop timer before destruction
    if (m_healthCheckTimer) {
        m_healthCheckTimer->stop();
    }

    // Stop health check and disconnect from server
    m_connected = false;
}

void ConnectionService::setServerUrl(const QString &url) {
    if (m_serverUrl != url) {
        m_serverUrl = url;
        emit serverUrlChanged();

        if (m_connected) {
            disconnect();
            connect();
        }
    }
}

void ConnectionService::testConnection() {
    performHealthCheck();
}

void ConnectionService::connect() {
    qDebug() << "[Connection Service] Connecting ... " << m_serverUrl;

    if (m_serverUrl.isEmpty()) {
        updateStatus(false, "No server URL set");
        return;
    }

    updateStatus(false, "Connecting ...");
    performHealthCheck();

    // start health check
    m_healthCheckTimer->start();
}

void ConnectionService::disconnect() {
    m_healthCheckTimer->stop();
    updateStatus(false, "Disconnected");
}

void ConnectionService::onHealthCheckComplete(bool success) {
    if (success) {
        updateStatus(true, "Connected");
    } else {
        updateStatus(false, "Connection failed");
    }
    emit connectionTested(success);
}

void ConnectionService::updateStatus(bool connected, const QString &text) {
    if (m_connected != connected) {
        m_connected = connected;
        emit connectedChanged();
    }

    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
}

void ConnectionService::performHealthCheck() {
    if (m_serverUrl.isEmpty()) {
        updateStatus(false, "No Server URL");
        return;
    }

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(m_serverUrl + "/health"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = manager->get(request);

    // Use QPointer to safely check if service still exists when callback executes
    QPointer<ConnectionService> servicePtr = this;

    QObject::connect(reply, &QNetworkReply::finished, [servicePtr, reply, manager]() {
        // Check if service still exists before accessing it
        if (!servicePtr) {
            // Service was destroyed, just clean up
            reply->deleteLater();
            manager->deleteLater();
            return;
        }

        bool success = false;
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                // qDebug() << "Connect Get: " << obj;
                if (obj.contains("status") && obj["status"].toString() == "healthy") {
                    success = true;
                }
            }
        }

        if (servicePtr) {
            servicePtr->onHealthCheckComplete(success);
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

}// namespace rbc