#pragma once
#include "RBCEditorRuntime/services/IConnectionService.h"
#include <QTimer>
#include <QString>

namespace rbc {

/**
 * Mock ConnectionService for testing
 * 
 * Provides controllable behavior for testing ConnectionViewModel and ConnectionPlugin
 * Inherits from ConnectionService to ensure compatibility
 */
class MockConnectionService : public IConnectionService {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit MockConnectionService(QObject *parent = nullptr);
    ~MockConnectionService() override = default;

    // Property accessors - hide base class methods to return mock state
    // Note: These are not virtual in base class, so we hide them instead
    QString serverUrl() const override;
    bool connected() const override;
    QString statusText() const override;
    void setServerUrl(const QString &url) override {};// nothing to do for mock Connection

    // Test control methods
    void simulateConnectionSuccess();
    void simulateConnectionFailure(const QString &errorMessage = "Connection failed");
    void simulateTestSuccess();
    void simulateTestFailure();
    void setAutoConnectDelay(int milliseconds);// Delay before auto-connecting
    void reset();

public slots:
    // Hide base class slots to provide mock behavior
    // Note: These are not virtual in base class, so we hide them instead
    void testConnection();
    void connect();
    void disconnect();

signals:
    void serverUrlChanged();
    void connectedChanged();
    void statusTextChanged();
    void connectionTested(bool success);

private slots:
    void onDelayedConnection();

private:
    // Mock state (shadow base class private members for testing)
    bool m_mockConnected = false;
    QString m_mockStatusText = "Disconnected";

    int m_autoConnectDelay = 0;
    QTimer *m_delayTimer = nullptr;
};

}// namespace rbc
