#include "ProjectPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"

#include <QQmlEngine>
#include <QDebug>
#include <QDir>

namespace rbc {

// ============================================================================
// ProjectPlugin Implementation
// ============================================================================

ProjectPlugin::ProjectPlugin(QObject *parent)
    : IEditorPlugin() {
    qDebug() << "ProjectPlugin created";
}

ProjectPlugin::~ProjectPlugin() {
    qDebug() << "ProjectPlugin destroyed";
}

bool ProjectPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ProjectPlugin::load: context is null";
        return false;
    }

    context_ = context;

    // Get ProjectService from PluginManager
    projectService_ = context->getService<IProjectService>();

    if (!projectService_) {
        qWarning() << "ProjectPlugin::load: ProjectService should be registered before ProjectPlugin load";
        return false;
    }

    // Create ViewModel
    viewModel_ = new ProjectViewModel(projectService_, this);

    qDebug() << "ProjectPlugin loaded successfully";
    return true;
}

bool ProjectPlugin::unload() {
    if (viewModel_) {
        viewModel_->deleteLater();
        viewModel_ = nullptr;
    }

    // Don't delete projectService_ as it might be used by other plugins
    projectService_ = nullptr;
    context_ = nullptr;

    qDebug() << "ProjectPlugin unloaded";
    return true;
}

bool ProjectPlugin::reload() {
    qDebug() << "ProjectPlugin reloading...";

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(context_)) {
        return false;
    }

    qDebug() << "ProjectPlugin reloaded";
    return true;
}

QList<ViewContribution> ProjectPlugin::view_contributions() const {
    ViewContribution view;
    view.viewId = "project_previewer";
    view.title = "Project Previewer";
    // qml file is compiled into the plugin's qrc under qrc:/qml/
    // keep the relative path minimal so WindowManager resolves it correctly
    view.qmlSource = "ProjectPreviewerView.qml";
    view.dockArea = "Left";
    view.preferredSize = "300,600";
    view.closable = true;
    view.movable = true;

    return {view};
}

void ProjectPlugin::register_view_models(QQmlEngine *engine) {
    if (!engine) {
        qWarning() << "ProjectPlugin::register_view_models: engine is null";
        return;
    }

    // Register ProjectViewModel as QML type
    qmlRegisterType<ProjectViewModel>("RoboCute.ProjectPreviewer", 1, 0, "ProjectViewModel");

    qDebug() << "ProjectPlugin: ViewModels registered";
}

QObject *ProjectPlugin::getViewModel(const QString &viewId) {
    if (viewId == "project_previewer" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

// Extern "C" Interface
IEditorPlugin *createPlugin() {
    // let upper parent manage the life cycle
    return static_cast<IEditorPlugin *>(new ProjectPlugin());
}

}// namespace rbc
