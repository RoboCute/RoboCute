#include "ChatClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>
#include <QDebug>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent), _network(new QNetworkAccessManager(this)) {
}

ChatClient::~ChatClient() {
    abortCurrentReply();
}

void ChatClient::setServerUrl(const QUrl &url) {
    _server_url = url;
}

void ChatClient::abortCurrentReply() {
    if (_current_reply) {
        _current_reply->abort();
        _current_reply->deleteLater();
        _current_reply = nullptr;
    }
}

void ChatClient::checkHealth() {
    if (!_server_url.isValid()) {
        emit connectedChanged(false);
        return;
    }

    QUrl healthUrl = _server_url;
    healthUrl.setPath(QStringLiteral("/health"));

    QNetworkRequest request(healthUrl);
    request.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = _network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        bool ok = reply->error() == QNetworkReply::NoError;
        if (ok) {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            ok = (status == 200);
        }
        if (_connected != ok) {
            _connected = ok;
            emit connectedChanged(_connected);
        }
        reply->deleteLater();
    });
}

void ChatClient::sendChat(const QJsonArray &messages, bool stream) {
    if (!_server_url.isValid()) {
        emit finished(false, QStringLiteral("Server URL is not set"));
        return;
    }

    abortCurrentReply();
    _buffer.clear();

    QUrl url = _server_url;
    url.setPath(QStringLiteral("/v1/chat/completions"));

    QJsonObject body;
    body[QStringLiteral("model")] = QStringLiteral("robocute-chat-demo");
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("stream")] = stream;
    body[QStringLiteral("max_tokens")] = 256;
    body[QStringLiteral("temperature")] = 0.7;

    QJsonDocument doc(body);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (stream) {
        request.setRawHeader("Accept", "text/event-stream");
    } else {
        request.setRawHeader("Accept", "application/json");
    }

    _current_reply = _network->post(request, data);
    _current_reply->setProperty("stream", stream);
    connect(_current_reply, &QNetworkReply::readyRead, this, &ChatClient::onReplyReadyRead);
    connect(_current_reply, &QNetworkReply::finished, this, &ChatClient::onReplyFinished);
    connect(_current_reply, &QNetworkReply::sslErrors, this, &ChatClient::onSslErrors);
}

void ChatClient::onReplyReadyRead() {
    if (!_current_reply) {
        return;
    }
    _buffer.append(_current_reply->readAll());
    parseSseBuffer();
}

void ChatClient::parseSseBuffer() {
    // SSE blocks are separated by a blank line ("\n\n").
    while (true) {
        int blockEnd = _buffer.indexOf("\n\n");
        if (blockEnd < 0) {
            break;
        }

        QByteArray block = _buffer.left(blockEnd);
        _buffer.remove(0, blockEnd + 2);

        for (const QByteArray &line : block.split('\n')) {
            QByteArray trimmed = line.trimmed();
            if (!trimmed.startsWith("data: ")) {
                continue;
            }

            QByteArray payload = trimmed.mid(6);
            if (payload == "[DONE]") {
                emit finished(true, QString());
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(payload);
            if (!doc.isObject()) {
                continue;
            }

            QJsonObject obj = doc.object();
            QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
            if (choices.isEmpty()) {
                continue;
            }

            QJsonObject choice = choices.first().toObject();
            QJsonObject delta = choice.value(QStringLiteral("delta")).toObject();
            QString content = delta.value(QStringLiteral("content")).toString();
            if (!content.isEmpty()) {
                emit tokenReceived(content);
            }

            if (!choice.value(QStringLiteral("finish_reason")).isNull()) {
                QString reason = choice.value(QStringLiteral("finish_reason")).toString();
                emit finished(true, reason);
                return;
            }
        }
    }
}

void ChatClient::onReplyFinished() {
    if (!_current_reply) {
        return;
    }

    QNetworkReply::NetworkError err = _current_reply->error();
    int status = _current_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (err == QNetworkReply::NoError || err == QNetworkReply::OperationCanceledError) {
        // Streaming responses usually end via [DONE] and never reach here with
        // a meaningful error; non-streaming responses need to be parsed.
        if (!_current_reply->property("stream").toBool()) {
            QByteArray data = _current_reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
                if (!choices.isEmpty()) {
                    QJsonObject message = choices.first().toObject()
                                             .value(QStringLiteral("message")).toObject();
                    QString content = message.value(QStringLiteral("content")).toString();
                    if (!content.isEmpty()) {
                        emit tokenReceived(content);
                    }
                }
            }
            emit finished(true, QString());
        } else {
            emit finished(true, QString());
        }
    } else {
        QByteArray data = _current_reply->readAll();
        QString detail = QString::fromUtf8(data);
        if (detail.isEmpty()) {
            detail = _current_reply->errorString();
        }
        emit finished(false, QStringLiteral("HTTP %1: %2").arg(status).arg(detail));
    }

    _current_reply->deleteLater();
    _current_reply = nullptr;
}

void ChatClient::onSslErrors(const QList<QSslError> &errors) {
    qWarning() << "ChatClient SSL errors:";
    for (const QSslError &e : errors) {
        qWarning() << "  " << e.errorString();
    }
    if (_current_reply) {
        _current_reply->ignoreSslErrors();
    }
}
