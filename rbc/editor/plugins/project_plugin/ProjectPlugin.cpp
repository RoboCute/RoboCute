#include "ProjectPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"

#include <QQmlEngine>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHeaderView>

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

    // Get ProjectService from PluginManager (using interface type)
    projectService_ = context->getService<IProjectService>();

    if (!projectService_) {
        qWarning() << "ProjectPlugin::load: ProjectService should be registered before ProjectPlugin load";
        return false;
    }

    // Create ViewModel
    viewModel_ = new ProjectViewModel(projectService_, this);

    // Create file browser widget
    fileBrowserWidget_ = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(fileBrowserWidget_);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QTreeView *treeView = new QTreeView(fileBrowserWidget_);
    treeView->setModel(viewModel_->fileSystemModel());
    treeView->setRootIndex(viewModel_->rootIndex());
    treeView->setHeaderHidden(false);
    treeView->setAlternatingRowColors(true);
    treeView->setAnimated(true);
    treeView->setIndentation(20);
    treeView->setSortingEnabled(true);
    
    // Connect double-click to navigate into directories
    QObject::connect(treeView, &QTreeView::doubleClicked, [this, treeView](const QModelIndex &index) {
        if (viewModel_ && viewModel_->isDirectory(index)) {
            QString path = viewModel_->getFilePath(index);
            viewModel_->setRootPath(path);
            treeView->setRootIndex(viewModel_->rootIndex());
        }
    });
    
    // Connect project opened signal to update tree view
    QObject::connect(projectService_, &IProjectService::projectOpened, [this, treeView]() {
        if (viewModel_) {
            viewModel_->setRootPath(projectService_->projectRoot());
            treeView->setRootIndex(viewModel_->rootIndex());
        }
    });
    
    layout->addWidget(treeView);
    fileBrowserWidget_->setLayout(layout);

    qDebug() << "ProjectPlugin loaded successfully";
    return true;
}

bool ProjectPlugin::unload() {
    if (fileBrowserWidget_) {
        fileBrowserWidget_->deleteLater();
        fileBrowserWidget_ = nullptr;
    }
    
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
    // We'll create the file browser widget directly, not via QML
    return {};
}

QList<MenuContribution> ProjectPlugin::menu_contributions() const {
    MenuContribution menuItem;
    menuItem.menuPath = "File/打开Project";  // Full path including action text
    menuItem.actionText = "打开Project";
    menuItem.actionId = "project.open";
    menuItem.shortcut = "Ctrl+O";
    // Use lambda to capture 'this' pointer
    // Note: We need to make sure 'this' is valid when callback is called
    // Since menu_contributions is const, we need to use mutable lambda or const_cast
    ProjectPlugin *nonConstThis = const_cast<ProjectPlugin *>(this);
    menuItem.callback = [nonConstThis]() {
        if (nonConstThis) {
            nonConstThis->onOpenProjectTriggered();
        }
    };
    
    return {menuItem};
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

void ProjectPlugin::onOpenProjectTriggered() {
    if (!projectService_) {
        qWarning() << "ProjectPlugin::onOpenProjectTriggered: projectService is null";
        return;
    }

    // Open file dialog to select project folder
    QString projectPath = QFileDialog::getExistingDirectory(
        nullptr,
        "选择Project文件夹",
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (projectPath.isEmpty()) {
        return; // User cancelled
    }

    // Open project
    ProjectOpenOptions options;
    options.loadUserPreferences = true;
    options.loadEditorSession = true;
    
    if (!projectService_->openProject(projectPath, options)) {
        qWarning() << "ProjectPlugin::onOpenProjectTriggered: Failed to open project:" 
                   << projectService_->lastError();
        return;
    }

    qDebug() << "ProjectPlugin: Project opened successfully:" << projectPath;

    // Update file browser widget if it exists
    if (fileBrowserWidget_) {
        if (viewModel_) {
            viewModel_->setRootPath(projectPath);
        }
    }
}

// Extern "C" Interface
IEditorPlugin *createPlugin() {
    // let upper parent manage the life cycle
    return static_cast<IEditorPlugin *>(new ProjectPlugin());
}

}// namespace rbc
