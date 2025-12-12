#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

namespace rbc {

class HttpClient;
class SceneSyncManager;

/**
 * Connection Status View Widget
 * 
 * Displays the current connection status between Editor and backend server.
 * Supports:
 * - Viewing current connection status
 * - Modifying server URL
 * - Testing server availability
 * - Reconnecting to server
 */
class ConnectionStatusView : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionStatusView(QWidget *parent = nullptr);
    ~ConnectionStatusView() override = default;

    // Set the HttpClient and SceneSyncManager references
    void setHttpClient(HttpClient *httpClient);
    void setSceneSyncManager(SceneSyncManager *sceneSyncManager);

    // Update connection status display
    void updateConnectionStatus(bool connected);
    
    // Update server URL display
    void updateServerUrl(const QString &url);

signals:
    // Emitted when user requests to reconnect with a new URL
    void reconnectRequested(const QString &serverUrl);

private slots:
    void onTestConnection();
    void onReconnect();

private:
    void setupUi();
    void updateStatusDisplay();

    HttpClient *httpClient_;
    SceneSyncManager *sceneSyncManager_;
    
    QLabel *statusLabel_;
    QLabel *statusIndicator_;
    QLineEdit *serverUrlEdit_;
    QPushButton *testButton_;
    QPushButton *reconnectButton_;
    
    bool isConnected_;
    bool isTesting_;
};

}  // namespace rbc

