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
    [[nodiscard]] QString serverUrl() const { return _server_url; }

    // API method

private:
    QNetworkAccessManager *_network_manager;
    QString _server_url;
    bool _is_connected;
};

}// namespace rbc