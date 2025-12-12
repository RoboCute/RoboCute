#include "RBCEditorRuntime/runtime/HttpClient.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

namespace rbc {
HttpClient::HttpClient(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_serverUrl("http://127.0.0.1:5555"), m_isConnected(false) {
}

HttpClient::~HttpClient() = default;

void HttpClient::setServerUrl(const QString &url) {
    m_serverUrl = url;
    if (m_serverUrl.endsWith('/')) {
        m_serverUrl.chop(1);
    }
}

void HttpClient::fetchAllNodes(std::function<void(const QJsonArray &, bool)> callback) {
    sendGetRequest("/nodes", [callback](const QJsonObject &response, bool success) {
        if (success && response.contains("nodes")) {
            QJsonValue nodesValue = response["nodes"];
            callback(nodesValue.toArray(), true);
        } else {
            callback(QJsonArray(), false);
        }
    });
}

void HttpClient::fetchNodeDetails(const QString &nodeType, std::function<void(const QJsonObject &, bool)> callback) {
    sendGetRequest("/nodes/" + nodeType, callback);
}

void HttpClient::executeGraph(const QJsonObject &graphDefinition, std::function<void(const QString &, bool)> callback) {
    QJsonObject requestBody;
    QJsonValue graphDefValue(graphDefinition);
    requestBody["graph_definition"] = graphDefValue;

    sendPostRequest("/graph/execute", requestBody, [callback](const QJsonObject &response, bool success) {
        if (success && response.contains("execution_id")) {
            callback(response["execution_id"].toString(), true);
        } else {
            callback(QString(), false);
        }
    });
}

void HttpClient::fetchExecutionOutputs(const QString &executionId, std::function<void(const QJsonObject &, bool)> callback) {
    sendGetRequest("/graph/" + executionId + "/outputs", callback);
}

void HttpClient::healthCheck(std::function<void(bool)> callback) {
    sendGetRequest("/health", [this, callback](const QJsonObject &response, bool success) {
        bool healthy = success && response.contains("status") && response["status"].toString() == "healthy";
        m_isConnected = healthy;
        emit connectionStatusChanged(healthy);
        callback(healthy);
    });
}

// Scene Synchronization Methods

void HttpClient::getSceneState(std::function<void(const QJsonObject &, bool)> callback) {
    sendGetRequest("/scene/state", callback);
}

void HttpClient::getAllResources(std::function<void(const QJsonObject &, bool)> callback) {
    sendGetRequest("/resources/all", callback);
}

void HttpClient::registerEditor(const QString &editorId, std::function<void(bool)> callback) {
    QJsonObject requestBody;
    requestBody["editor_id"] = editorId;

    sendPostRequest("/editor/register", requestBody, [callback](const QJsonObject &response, bool success) {
        bool registered = success && response.contains("success") && response["success"].toBool();
        callback(registered);
    });
}

void HttpClient::sendHeartbeat(const QString &editorId, std::function<void(bool)> callback) {
    QJsonObject requestBody;
    requestBody["editor_id"] = editorId;

    sendPostRequest("/editor/heartbeat", requestBody, [callback](const QJsonObject &response, bool success) {
        bool acknowledged = success && response.contains("success") && response["success"].toBool();
        callback(acknowledged);
    });
}

// Animation Methods

void HttpClient::getAnimations(std::function<void(const QJsonObject &, bool)> callback) {
    sendGetRequest("/animations/all", callback);
}

void HttpClient::getAnimationData(const QString &name, std::function<void(const QJsonObject &, bool)> callback) {
    QString endpoint = QString("/animations/%1").arg(name);
    sendGetRequest(endpoint, callback);
}

void HttpClient::sendGetRequest(const QString &endpoint, std::function<void(const QJsonObject &, bool)> callback) {
    QUrl url(m_serverUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

            if (jsonDoc.isObject()) {
                callback(jsonDoc.object(), true);
            } else if (jsonDoc.isArray()) {
                QJsonObject wrapper;
                wrapper["data"] = jsonDoc.array();
                callback(wrapper, true);
            } else {
                emit errorOccurred("Invalid JSON response");
                callback(QJsonObject(), false);
            }
        } else {
            handleReplyError(reply);
            callback(QJsonObject(), false);
        }
        reply->deleteLater();
    });
}

void HttpClient::sendPostRequest(const QString &endpoint, const QJsonObject &data, std::function<void(const QJsonObject &, bool)> callback) {
    QUrl url(m_serverUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonDocument jsonDoc(data);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkReply *reply = m_networkManager->post(request, jsonData);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);

            if (jsonDoc.isObject()) {
                callback(jsonDoc.object(), true);
            } else {
                emit errorOccurred("Invalid JSON response");
                callback(QJsonObject(), false);
            }
        } else {
            handleReplyError(reply);
            callback(QJsonObject(), false);
        }
        reply->deleteLater();
    });
}

void HttpClient::handleReplyError(const QNetworkReply *reply) {
    QString errorMsg;
    switch (reply->error()) {
        case QNetworkReply::ConnectionRefusedError:
            errorMsg = "Connection refused. Is the backend server running?";
            break;
        case QNetworkReply::RemoteHostClosedError:
            errorMsg = "Remote host closed the connection.";
            break;
        case QNetworkReply::HostNotFoundError:
            errorMsg = "Host not found.";
            break;
        case QNetworkReply::TimeoutError:
            errorMsg = "Request timeout.";
            break;
        case QNetworkReply::ContentNotFoundError:
            errorMsg = "Content not found (404).";
            break;
        default:
            errorMsg = QString("Network error: %1").arg(reply->errorString());
            break;
    }

    emit errorOccurred(errorMsg);
    m_isConnected = false;
    emit connectionStatusChanged(false);
}

}// namespace rbc
