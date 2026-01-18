#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <functional>

struct QNetworkAccessManager;
struct QNetworkReply;

namespace rbc {

class RBC_EDITOR_RUNTIME_API HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);
    ~HttpClient() override;

public:
    void setServerUrl(const QString &url);
    [[nodiscard]] QString serverUrl() const { return m_serverUrl; }

    // API method

private:
    QNetworkAccessManager *m_networkManager;
    QString m_serverUrl;
    bool m_isConnected;
};

}// namespace rbc