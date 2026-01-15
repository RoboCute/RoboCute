#pragma once
#include <rbc_config.h>
#include <QObject>
#include "RBCEditorRuntime/services/SceneService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

namespace rbc {

/**
 * ViewportViewModel Viewport状态视图
 * - Camera State
 * - Control Command State
 * - Show/Hide State
 * - RenderConfig
 */
class ViewportViewModel : public ViewModelBase {
    Q_OBJECT

public:
    explicit ViewportViewModel(ISceneService *scenService, QObject *parent = nullptr);
    ~ViewportViewModel();

private:
    ISceneService *sceneService_;
};

class RBC_EDITOR_RUNTIME_API ViewportPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit ViewportPlugin(QObject *parent = nullptr);
    ~ViewportPlugin() override;

    // === Static Methods for Factory ===
    static QString staticPluginId() { return "com.robocute.viewport"; }
    static QString staticPluginName() { return "Viewport Plugin"; }

    // === IEditorPlugin Interface ===
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }
    QList<ViewContribution> view_contributions() const override { return {}; }
    QList<MenuContribution> menu_contributions() const override { return {}; }
    QList<ToolbarContribution> toolbar_contributions() const override {
        return {};
    }
    void register_view_models(QQmlEngine *engine) override {}
    QObject *getViewModel(const QString &viewId) override;

private:
    ISceneService *sceneService_ = nullptr;
    ViewportViewModel *viewModel_ = nullptr;
    PluginContext *context_ = nullptr;
};

}// namespace rbc