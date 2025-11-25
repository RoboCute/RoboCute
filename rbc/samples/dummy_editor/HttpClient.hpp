#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <functional>

struct QNetworkAccessManager;
struct QNetworkReply;

/**
 * HTTP client for communicating with the FastAPI backend
 */
struct HttpClient : public QObject
{
    Q_OBJECT

public:
    explicit HttpClient(QObject *parent = nullptr);
    ~HttpClient() override;

    // Set the backend server URL
    void setServerUrl(const QString &url);
    QString serverUrl() const { return m_serverUrl; }

    // API methods
    void fetchAllNodes(std::function<void(const QJsonArray &, bool)> callback);
    void fetchNodeDetails(const QString &nodeType, std::function<void(const QJsonObject &, bool)> callback);
    void executeGraph(const QJsonObject &graphDefinition, std::function<void(const QString &executionId, bool)> callback);
    void fetchExecutionOutputs(const QString &executionId, std::function<void(const QJsonObject &, bool)> callback);
    void healthCheck(std::function<void(bool)> callback);

signals:
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString &errorMessage);

private:
    void sendGetRequest(const QString &endpoint, std::function<void(const QJsonObject &, bool)> callback);
    void sendPostRequest(const QString &endpoint, const QJsonObject &data, std::function<void(const QJsonObject &, bool)> callback);
    void handleReplyError(const QNetworkReply *reply);

    QNetworkAccessManager *m_networkManager;
    QString m_serverUrl;
    bool m_isConnected;
};

