#include "RBCEditorRuntime/services/ConnectionService.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace rbc {

ConnectionService::ConnectionService(QObject *parent) : QObject(parent) {
    m_healthCheckTimer = new QTimer(this);
    m_healthCheckTimer->setInterval(5000);
    QObject::connect(m_healthCheckTimer, &QTimer::timeout, this, &ConnectionService::performHealthCheck);
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

    QObject::connect(reply, &QNetworkReply::finished, [this, reply, manager]() {
        bool success = false;
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("status") && obj["status"].toString() == "ok") {
                    success = true;
                }
            }
        }

        onHealthCheckComplete(success);
        reply->deleteLater();
        manager->deleteLater();
    });
}

}// namespace rbc