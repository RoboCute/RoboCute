#pragma once

#include <QJsonArray>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
QT_END_NAMESPACE

class ChatClient;

/**
 * @brief Native chat widget for the RoboCute chat demo plugin.
 */
class ChatDemoWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatDemoWidget(ChatClient *client, QWidget *parent = nullptr);
    ~ChatDemoWidget() override;

    void setServerUrl(const QString &url);

private slots:
    void onSendClicked();
    void onInputReturnPressed();
    void onTokenReceived(const QString &token);
    void onFinished(bool ok, const QString &error);
    void onConnectedChanged(bool connected);
    void checkHealth();

private:
    void appendMessage(const QString &role, const QString &text);
    void scrollToBottom();
    void updateStatus(bool connected, const QString &hint = QString());

    ChatClient *_client = nullptr;
    QJsonArray _messages;

    QTextEdit *_history = nullptr;
    QLineEdit *_input = nullptr;
    QPushButton *_send_button = nullptr;
    QLabel *_status_label = nullptr;
    QCheckBox *_stream_checkbox = nullptr;

    bool _awaiting_reply = false;
    bool _last_was_assistant = false;
    QString _current_assistant_text;
};
