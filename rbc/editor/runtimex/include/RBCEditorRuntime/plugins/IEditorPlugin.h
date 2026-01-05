#pragma once

#include <QObject>
#include <QQmlEngine>

namespace rbc {

struct PluginContext;

enum struct PluginState {
};

struct ViewContribution {
    QString viewId;   // the unique identifier
    QString title;    // display title
    QString qmlSource;// relevant path for QML
    QString dockArea; // (Left/Right/Top/Bottom/Center/...)
    QString preferredSize;
    bool closable;
    bool movable;
};

struct MenuContribution {};
struct ToolbarContribution {};

class IEditorPlugin : public QObject {
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

signals:
    void reloadRequested();
    void stateChanged(PluginState state);
};

}// namespace rbc