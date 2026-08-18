#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
QT_END_NAMESPACE

/**
 * @brief Small QObject wrapper around QNetworkAccessManager for the chat demo.
 *
 * Sends OpenAI-compatible chat-completion requests and parses Server-Sent
 * Events (SSE) so the UI can stream tokens as they arrive.
 */
class ChatClient : public QObject {
    Q_OBJECT
public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient() override;

    void setServerUrl(const QUrl &url);
    QUrl serverUrl() const { return _server_url; }

    void checkHealth();
    void sendChat(const QJsonArray &messages, bool stream = true);

    bool isBusy() const { return _current_reply != nullptr; }

signals:
    void tokenReceived(const QString &token);
    void finished(bool ok, const QString &error);
    void connectedChanged(bool connected);

private slots:
    void onReplyReadyRead();
    void onReplyFinished();
    void onSslErrors(const QList<QSslError> &errors);

private:
    void parseSseBuffer();
    void abortCurrentReply();

    QNetworkAccessManager *_network = nullptr;
    QUrl _server_url;
    QPointer<QNetworkReply> _current_reply;
    QByteArray _buffer;
    bool _connected = false;
    bool _busy = false;
};
