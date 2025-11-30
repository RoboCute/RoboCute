#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <functional>

struct QNetworkAccessManager;
struct QNetworkReply;

namespace rbc {

/**
 * HTTP client for communicating with the FastAPI backend
 */
struct HttpClient : public QObject {
    Q_OBJECT

public:
    explicit HttpClient(QObject *parent = nullptr);
    ~HttpClient() override;

    // Set the backend server URL
    void setServerUrl(const QString &url);
    [[nodiscard]] QString serverUrl() const { return m_serverUrl; }

    // API methods - Node Execution
    void fetchAllNodes(std::function<void(const QJsonArray &, bool)> callback);
    void fetchNodeDetails(const QString &nodeType, std::function<void(const QJsonObject &, bool)> callback);
    void executeGraph(const QJsonObject &graphDefinition, std::function<void(const QString &executionId, bool)> callback);
    void fetchExecutionOutputs(const QString &executionId, std::function<void(const QJsonObject &, bool)> callback);
    void healthCheck(std::function<void(bool)> callback);

    // API methods - Scene Synchronization
    void getSceneState(std::function<void(const QJsonObject &, bool)> callback);
    void getAllResources(std::function<void(const QJsonObject &, bool)> callback);
    void registerEditor(const QString &editorId, std::function<void(bool)> callback);
    void sendHeartbeat(const QString &editorId, std::function<void(bool)> callback);
    
    // API methods - Animation
    void getAnimations(std::function<void(const QJsonObject &, bool)> callback);
    void getAnimationData(const QString &name, std::function<void(const QJsonObject &, bool)> callback);

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

}// namespace rbc
