#pragma once

#include <rbc_config.h>
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"
#include <QPointer>

namespace rbc {
class PluginContext;
}

class ChatClient;
class ChatDemoWidget;

namespace rbc {

/**
 * @brief Independent chat demo plugin for RoboCute.
 *
 * Provides a native QWidget chat view that talks to the Python chat server over
 * HTTP/SSE. This is intentionally kept separate from the main editor layout so
 * it can be verified standalone.
 */
class ChatDemoPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit ChatDemoPlugin(QObject *parent = nullptr);
    ~ChatDemoPlugin() override;

    static QString staticPluginId() { return QStringLiteral("com.robocute.chat_demo"); }
    static QString staticPluginName() { return QStringLiteral("LLM Chat Demo"); }

    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return QStringLiteral("0.1.0"); }
    QStringList dependencies() const override { return {}; }

    QList<ViewContribution> view_contributions() const override { return {}; }
    QList<NativeViewContribution> native_view_contributions() const override;
    QList<MenuContribution> menu_contributions() const override { return {}; }
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }

    void register_view_models(QQmlEngine *engine) override { Q_UNUSED(engine); }
    QWidget *getNativeWidget(const QString &viewId) override;

private:
    PluginContext *_context = nullptr;
    ChatClient *_client = nullptr;
    QPointer<ChatDemoWidget> _widget;
    QList<NativeViewContribution> _contributions;
};

LUISA_EXPORT_API IPluginFactory *createPluginFactory();

} // namespace rbc
