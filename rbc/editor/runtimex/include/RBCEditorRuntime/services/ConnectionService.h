#pragma once

#include <rbc_config.h>
#include <QObject>
#include <QTimer>
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API ConnectionService : public IService {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit ConnectionService(QObject *parent = nullptr);
    ~ConnectionService() override;

    // IService interface
    QString serviceId() const override { return "com.robocute.connection_service"; }

    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &url);

    bool connected() const { return m_connected; }
    QString statusText() const { return m_statusText; }

public slots:
    void testConnection();
    void connect();
    void disconnect();

signals:
    void serverUrlChanged();
    void connectedChanged();
    void statusTextChanged();
    void connectionTested(bool success);

private slots:
    void onHealthCheckComplete(bool success);

private:
    void updateStatus(bool connected, const QString &text);
    void performHealthCheck();

private:
    QString m_serverUrl = "http://127.0.0.1:5555";
    bool m_connected = false;
    QString m_statusText = "Disconnected";
    QTimer *m_healthCheckTimer = nullptr;
};

}// namespace rbc