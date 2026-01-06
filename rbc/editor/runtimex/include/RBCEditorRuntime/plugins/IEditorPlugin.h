#pragma once

#include <rbc_config.h>
#include <QObject>
#include <QQmlEngine>
#include "RBCEditorRuntime/plugins/PluginContributions.h"

namespace rbc {

struct PluginContext;

struct MenuContribution {};
struct ToolbarContribution {};

class RBC_EDITOR_RUNTIME_API IEditorPlugin : public QObject {
    Q_OBJECT
public:
    virtual ~IEditorPlugin() = default;

    // === Life Cycle ===
    virtual bool load(PluginContext *context) = 0;
    virtual bool unload() = 0;
    virtual bool reload() = 0;// hot-reload support

    // === Meta Data ===
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QStringList dependencies() const = 0;

    // === UI Contributions ===
    virtual QList<ViewContribution> view_contributions() const = 0;
    virtual QList<MenuContribution> menu_contributions() const = 0;
    virtual QList<ToolbarContribution> toolbar_contributions() const = 0;

    // == ViewModel Registration ==
    virtual void register_view_models(QQmlEngine *engine) = 0;

    // == Get ViewModel for a view ==
    virtual QObject *getViewModel(const QString &viewId) {
        Q_UNUSED(viewId);
        return nullptr;
    }

signals:
    void reloadRequested();
    void stateChanged(PluginState state);
};

}// namespace rbc