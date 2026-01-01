#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace rbc {

/**
 * ConnectionService - QML 连接状态服务
 * 提供连接状态管理和健康检查功能
 */
class ConnectionService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit ConnectionService(QObject *parent = nullptr);
    ~ConnectionService() override = default;

    QString serverUrl() const { return m_serverUrl; }
    void setServerUrl(const QString &url);

    bool connected() const { return m_connected; }
    QString statusText() const { return m_statusText; }

public slots:
    /**
     * 测试连接
     */
    void testConnection();

    /**
     * 开始连接
     */
    void connect();

    /**
     * 断开连接
     */
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

    QString m_serverUrl = "http://127.0.0.1:5555";
    bool m_connected = false;
    QString m_statusText = "Disconnected";
    QTimer *m_healthCheckTimer = nullptr;
};

} // namespace rbc

