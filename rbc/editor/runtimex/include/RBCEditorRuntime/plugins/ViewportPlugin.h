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
};

class ViewportPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit ViewportPlugin(QObject *parent = nullptr);
    ~ViewportPlugin() override;

    // === IEditorPlugin Interface ===
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return "com.robocute.viewport"; }
    QString name() const override { return "Viewport Plugin"; }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }
    bool is_dynamic() const override { return false; }
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