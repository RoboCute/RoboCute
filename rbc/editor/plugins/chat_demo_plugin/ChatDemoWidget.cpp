#include "ChatDemoWidget.h"
#include "ChatClient.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QDebug>

ChatDemoWidget::ChatDemoWidget(ChatClient *client, QWidget *parent)
    : QWidget(parent), _client(client) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    _history = new QTextEdit(this);
    _history->setReadOnly(true);
    _history->setPlaceholderText(tr("Chat history will appear here..."));
    mainLayout->addWidget(_history, 1);

    auto *inputLayout = new QHBoxLayout();
    _input = new QLineEdit(this);
    _input->setPlaceholderText(tr("Type a message and press Enter to send"));
    inputLayout->addWidget(_input, 1);

    _send_button = new QPushButton(tr("Send"), this);
    inputLayout->addWidget(_send_button);

    _stream_checkbox = new QCheckBox(tr("Stream"), this);
    _stream_checkbox->setChecked(true);
    inputLayout->addWidget(_stream_checkbox);

    mainLayout->addLayout(inputLayout);

    auto *statusLayout = new QHBoxLayout();
    _status_label = new QLabel(tr("Server: unknown"), this);
    statusLayout->addWidget(_status_label);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    connect(_send_button, &QPushButton::clicked, this, &ChatDemoWidget::onSendClicked);
    connect(_input, &QLineEdit::returnPressed, this, &ChatDemoWidget::onInputReturnPressed);

    if (_client) {
        connect(_client, &ChatClient::tokenReceived, this, &ChatDemoWidget::onTokenReceived);
        connect(_client, &ChatClient::finished, this, &ChatDemoWidget::onFinished);
        connect(_client, &ChatClient::connectedChanged, this, &ChatDemoWidget::onConnectedChanged);
    }

    // Poll server health every 3 seconds.
    auto *healthTimer = new QTimer(this);
    connect(healthTimer, &QTimer::timeout, this, &ChatDemoWidget::checkHealth);
    healthTimer->start(3000);
    QTimer::singleShot(100, this, &ChatDemoWidget::checkHealth);
}

ChatDemoWidget::~ChatDemoWidget() = default;

void ChatDemoWidget::setServerUrl(const QString &url) {
    if (_client) {
        _client->setServerUrl(QUrl(url));
        checkHealth();
    }
}

void ChatDemoWidget::onSendClicked() {
    QString text = _input->text().trimmed();
    if (text.isEmpty() || _awaiting_reply) {
        return;
    }

    _input->clear();
    appendMessage(QStringLiteral("User"), text);

    QJsonObject userMsg;
    userMsg[QStringLiteral("role")] = QStringLiteral("user");
    userMsg[QStringLiteral("content")] = text;
    _messages.append(userMsg);

    _awaiting_reply = true;
    _send_button->setEnabled(false);
    _last_was_assistant = false;
    _current_assistant_text.clear();

    appendMessage(QStringLiteral("Assistant"), QString());

    if (_client) {
        _client->sendChat(_messages, _stream_checkbox->isChecked());
    }
}

void ChatDemoWidget::onInputReturnPressed() {
    onSendClicked();
}

void ChatDemoWidget::onTokenReceived(const QString &token) {
    if (!_last_was_assistant) {
        _last_was_assistant = true;
    }
    _current_assistant_text += token;

    // Append to the last line of the history.
    QTextCursor cursor = _history->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(token);
    scrollToBottom();
}

void ChatDemoWidget::onFinished(bool ok, const QString &error) {
    _awaiting_reply = false;
    _send_button->setEnabled(true);

    if (!ok) {
        appendMessage(QStringLiteral("System"), QStringLiteral("Error: %1").arg(error));
    } else if (!_current_assistant_text.isEmpty()) {
        QJsonObject assistantMsg;
        assistantMsg[QStringLiteral("role")] = QStringLiteral("assistant");
        assistantMsg[QStringLiteral("content")] = _current_assistant_text;
        _messages.append(assistantMsg);
    }
    _current_assistant_text.clear();
}

void ChatDemoWidget::onConnectedChanged(bool connected) {
    updateStatus(connected);
}

void ChatDemoWidget::checkHealth() {
    if (_client) {
        _client->checkHealth();
    }
}

void ChatDemoWidget::appendMessage(const QString &role, const QString &text) {
    QString prefix;
    if (role == QStringLiteral("User")) {
        prefix = QStringLiteral("\n<b>User:</b> ");
    } else if (role == QStringLiteral("Assistant")) {
        prefix = QStringLiteral("\n<b>Assistant:</b> ");
    } else {
        prefix = QStringLiteral("\n<i>%1:</i> ").arg(role);
    }

    _history->append(prefix + text);
    scrollToBottom();
}

void ChatDemoWidget::scrollToBottom() {
    QScrollBar *bar = _history->verticalScrollBar();
    if (bar) {
        bar->setValue(bar->maximum());
    }
}

void ChatDemoWidget::updateStatus(bool connected, const QString &hint) {
    QString status;
    if (connected) {
        status = tr("Server: connected");
        _status_label->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        status = tr("Server: disconnected");
        _status_label->setStyleSheet(QStringLiteral("color: red;"));
    }
    if (!hint.isEmpty()) {
        status += QStringLiteral(" - ") + hint;
    }
    _status_label->setText(status);
}
