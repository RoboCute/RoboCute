#include "RBCEditorRuntime/services/ConnectionService.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>

namespace rbc {

ConnectionService::ConnectionService(QObject *parent) : IConnectionService(parent) {
    _health_check_timer = new QTimer(this);
    _health_check_timer->setInterval(5000);
    QObject::connect(_health_check_timer, &QTimer::timeout, this, &ConnectionService::performHealthCheck);
}

ConnectionService::~ConnectionService() {
    // Stop timer before destruction
    if (_health_check_timer) {
        _health_check_timer->stop();
    }

    // Stop health check and disconnect from server
    _connected = false;
}

void ConnectionService::setServerUrl(const QString &url) {
    if (_server_url != url) {
        _server_url = url;
        emit serverUrlChanged();

        if (_connected) {
            disconnect();
            connect();
        }
    }
}

void ConnectionService::testConnection() {
    performHealthCheck();
}

void ConnectionService::connect() {
    qDebug() << "[Connection Service] Connecting ... " << _server_url;

    if (_server_url.isEmpty()) {
        updateStatus(false, "No server URL set");
        return;
    }

    updateStatus(false, "Connecting ...");
    performHealthCheck();

    // start health check
    _health_check_timer->start();
}

void ConnectionService::disconnect() {
    _health_check_timer->stop();
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
    if (_connected != connected) {
        _connected = connected;
        emit connectedChanged();
    }

    if (_status_text != text) {
        _status_text = text;
        emit statusTextChanged();
    }
}

void ConnectionService::performHealthCheck() {
    if (_server_url.isEmpty()) {
        updateStatus(false, "No Server URL");
        return;
    }

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(_server_url + "/health"));
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