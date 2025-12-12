#include "RBCEditorRuntime/components/ConnectionStatusView.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include <QTimer>
#include <QMessageBox>

namespace rbc {

ConnectionStatusView::ConnectionStatusView(QWidget *parent)
    : QWidget(parent),
      httpClient_(nullptr),
      sceneSyncManager_(nullptr),
      isConnected_(false),
      isTesting_(false) {
    setupUi();
}

void ConnectionStatusView::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Title
    auto *titleLabel = new QLabel("Connection Status", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Status indicator and label
    auto *statusLayout = new QHBoxLayout();
    statusIndicator_ = new QLabel(this);
    statusIndicator_->setFixedSize(16, 16);
    statusIndicator_->setStyleSheet("background-color: gray; border-radius: 8px;");
    
    statusLabel_ = new QLabel("Disconnected", this);
    statusLabel_->setStyleSheet("color: gray;");
    
    statusLayout->addWidget(statusIndicator_);
    statusLayout->addWidget(statusLabel_);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // Server URL input
    auto *formLayout = new QFormLayout();
    serverUrlEdit_ = new QLineEdit(this);
    serverUrlEdit_->setPlaceholderText("http://127.0.0.1:5555");
    serverUrlEdit_->setText("http://127.0.0.1:5555");
    formLayout->addRow("Server URL:", serverUrlEdit_);
    mainLayout->addLayout(formLayout);

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    
    testButton_ = new QPushButton("Test Connection", this);
    testButton_->setToolTip("Test if the server is available");
    connect(testButton_, &QPushButton::clicked, this, &ConnectionStatusView::onTestConnection);
    
    reconnectButton_ = new QPushButton("Reconnect", this);
    reconnectButton_->setToolTip("Reconnect to the server");
    connect(reconnectButton_, &QPushButton::clicked, this, &ConnectionStatusView::onReconnect);
    
    buttonLayout->addWidget(testButton_);
    buttonLayout->addWidget(reconnectButton_);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();
    setLayout(mainLayout);
    
    updateStatusDisplay();
}

void ConnectionStatusView::setHttpClient(HttpClient *httpClient) {
    httpClient_ = httpClient;
    if (httpClient_) {
        updateServerUrl(httpClient_->serverUrl());
    }
}

void ConnectionStatusView::setSceneSyncManager(SceneSyncManager *sceneSyncManager) {
    sceneSyncManager_ = sceneSyncManager;
    if (sceneSyncManager_) {
        updateConnectionStatus(sceneSyncManager_->isConnected());
    }
}

void ConnectionStatusView::updateConnectionStatus(bool connected) {
    isConnected_ = connected;
    updateStatusDisplay();
}

void ConnectionStatusView::updateServerUrl(const QString &url) {
    if (serverUrlEdit_->text() != url) {
        serverUrlEdit_->setText(url);
    }
}

void ConnectionStatusView::updateStatusDisplay() {
    if (isTesting_) {
        statusIndicator_->setStyleSheet("background-color: yellow; border-radius: 8px;");
        statusLabel_->setText("Testing...");
        statusLabel_->setStyleSheet("color: orange;");
        testButton_->setEnabled(false);
        reconnectButton_->setEnabled(false);
    } else if (isConnected_) {
        statusIndicator_->setStyleSheet("background-color: green; border-radius: 8px;");
        statusLabel_->setText("Connected");
        statusLabel_->setStyleSheet("color: green;");
        testButton_->setEnabled(true);
        reconnectButton_->setEnabled(true);
    } else {
        statusIndicator_->setStyleSheet("background-color: red; border-radius: 8px;");
        statusLabel_->setText("Disconnected");
        statusLabel_->setStyleSheet("color: red;");
        testButton_->setEnabled(true);
        reconnectButton_->setEnabled(true);
    }
}

void ConnectionStatusView::onTestConnection() {
    if (!httpClient_) {
        QMessageBox::warning(this, "Error", "HttpClient is not set.");
        return;
    }

    QString url = serverUrlEdit_->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a server URL.");
        return;
    }

    // Update HttpClient URL temporarily for testing
    QString originalUrl = httpClient_->serverUrl();
    httpClient_->setServerUrl(url);

    // Set testing state
    isTesting_ = true;
    updateStatusDisplay();

    // Perform health check
    httpClient_->healthCheck([this, originalUrl](bool success) {
        isTesting_ = false;
        
        // Restore original URL
        httpClient_->setServerUrl(originalUrl);
        
        updateStatusDisplay();
        
        if (success) {
            QMessageBox::information(this, "Success", 
                QString("Server at %1 is available and healthy.").arg(serverUrlEdit_->text()));
        } else {
            QMessageBox::warning(this, "Failed", 
                QString("Cannot connect to server at %1.\nPlease check if the server is running.").arg(serverUrlEdit_->text()));
        }
    });
}

void ConnectionStatusView::onReconnect() {
    QString url = serverUrlEdit_->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a server URL.");
        return;
    }

    // Update HttpClient URL
    if (httpClient_) {
        httpClient_->setServerUrl(url);
    }

    // Emit signal to request reconnection
    emit reconnectRequested(url);
}

}  // namespace rbc

