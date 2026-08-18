#include "ChatDemoPlugin.h"
#include "ChatClient.h"
#include "ChatDemoWidget.h"

#include "RBCEditorRuntime/plugins/IPluginFactory.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include <QDebug>
#include <QProcessEnvironment>

namespace rbc {

ChatDemoPlugin::ChatDemoPlugin(QObject *parent)
    : IEditorPlugin(parent), _client(new ChatClient(this)) {
    qDebug() << "ChatDemoPlugin created";
}

ChatDemoPlugin::~ChatDemoPlugin() {
    if (_widget) {
        qWarning() << "ChatDemoPlugin::~ChatDemoPlugin: unload() was not called before destruction";
        if (_widget->parent() == nullptr) {
            delete _widget.data();
        }
    }
    qDebug() << "ChatDemoPlugin destroyed";
}

bool ChatDemoPlugin::load(rbc::PluginContext *context) {
    if (!context) {
        qWarning() << "ChatDemoPlugin::load: context is null";
        return false;
    }
    _context = context;

    // Determine server URL from environment or fall back to the default port.
    QString serverUrl = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("RBC_CHAT_SERVER_URL"),
        QStringLiteral("http://127.0.0.1:8123"));
    _client->setServerUrl(QUrl(serverUrl));

    _widget = new ChatDemoWidget(_client, nullptr);
    _widget->setServerUrl(serverUrl);

    rbc::NativeViewContribution contribution;
    contribution.viewId = QStringLiteral("chat_demo");
    contribution.title = QStringLiteral("Chat Demo");
    contribution.dockArea = QStringLiteral("Right");
    contribution.closable = true;
    contribution.movable = true;
    contribution.floatable = true;
    contribution.isExternalManaged = true;// Plugin owns the widget lifecycle

    _contributions.append(contribution);

    qDebug() << "ChatDemoPlugin loaded; server URL:" << serverUrl;
    return true;
}

bool ChatDemoPlugin::unload() {
    qDebug() << "ChatDemoPlugin::unload";

    // Detach widget from any dock / main window before deleting it.
    if (_widget) {
        _widget->setParent(nullptr);
        delete _widget.data();
    }

    _contributions.clear();
    _context = nullptr;
    return true;
}

bool ChatDemoPlugin::reload() {
    qDebug() << "ChatDemoPlugin::reload";
    if (!unload()) {
        return false;
    }
    return load(_context);
}

QList<rbc::NativeViewContribution> ChatDemoPlugin::native_view_contributions() const {
    return _contributions;
}

QWidget *ChatDemoPlugin::getNativeWidget(const QString &viewId) {
    if (viewId == QStringLiteral("chat_demo")) {
        return _widget.data();
    }
    return nullptr;
}

// Factory exported from the plugin DLL.
class ChatDemoPluginFactory : public IPluginFactory {
public:
    [[nodiscard]] std::unique_ptr<IEditorPlugin> create() override {
        return std::make_unique<ChatDemoPlugin>();
    }

    [[nodiscard]] QString pluginId() const override {
        return ChatDemoPlugin::staticPluginId();
    }

    [[nodiscard]] QString pluginName() const override {
        return ChatDemoPlugin::staticPluginName();
    }
};

LUISA_EXPORT_API IPluginFactory *createPluginFactory() {
    return new ChatDemoPluginFactory();
}

} // namespace rbc
