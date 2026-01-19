#pragma once

#include <rbc_config.h>
#include <QObject>
#include <QTimer>
#include "RBCEditorRuntime/services/IConnectionService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API ConnectionService : public IConnectionService {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit ConnectionService(QObject *parent = nullptr);
    ~ConnectionService();

    // IService interface
    QString serviceId() const override { return "com.robocute.connection_service"; }

    // IConnectionServiceInterface
    QString serverUrl() const override { return m_serverUrl; }
    void setServerUrl(const QString &url) override;
    bool connected() const override { return m_connected; }
    QString statusText() const override { return m_statusText; }

public slots:
    void testConnection();
    void connect();
    void disconnect();

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