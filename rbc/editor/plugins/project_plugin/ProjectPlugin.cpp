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
#include <QPointer>
#include <QDockWidget>

namespace rbc {

// ============================================================================
// ProjectPlugin Implementation
// ============================================================================

ProjectPlugin::ProjectPlugin(QObject *parent)
    : IEditorPlugin() {
    qDebug() << "ProjectPlugin created";
}

ProjectPlugin::~ProjectPlugin() {
    // Clean up in reverse order of creation
    // First disconnect all signals
    if (projectService_) {
        QObject::disconnect(projectService_, nullptr, this, nullptr);
    }
    
    // CRITICAL: fileBrowserWidget_ ownership issue
    // Once fileBrowserWidget_ is added to DockWidget, the dock owns it.
    // We should NOT delete it here, just clean up references.
    if (fileBrowserWidget_) {
        QWidget *parent = fileBrowserWidget_->parentWidget();
        if (parent) {
            // Widget has a parent (likely DockWidget), so it's owned by the parent
            // We should NOT delete it, just clean up our references
            QDockWidget *dock = qobject_cast<QDockWidget *>(parent);
            if (dock) {
                // Clear model from tree view
                QTreeView *treeView = fileBrowserWidget_->findChild<QTreeView *>();
                if (treeView) {
                    treeView->setModel(nullptr);
                    treeView->disconnect();
                }
                
                // Disconnect signals
                fileBrowserWidget_->disconnect();
                
                // Remove widget from dock (dock will handle deletion)
                dock->setWidget(nullptr);
                qDebug() << "ProjectPlugin::~ProjectPlugin: Released fileBrowserWidget ownership to DockWidget";
            } else {
                qDebug() << "ProjectPlugin::~ProjectPlugin: fileBrowserWidget has parent, releasing ownership";
            }
        } else {
            // Widget has no parent, we own it and should delete it
            QTreeView *treeView = fileBrowserWidget_->findChild<QTreeView *>();
            if (treeView) {
                treeView->setModel(nullptr);
                treeView->disconnect();
            }
            fileBrowserWidget_->disconnect();
            fileBrowserWidget_->deleteLater();
            qDebug() << "ProjectPlugin::~ProjectPlugin: Deleted fileBrowserWidget (no parent)";
        }
        
        // Always clear our pointer
        fileBrowserWidget_ = nullptr;
    }
    
    // Clean up ViewModel
    if (viewModel_) {
        viewModel_->deleteLater();
        viewModel_ = nullptr;
    }
    
    // Clear references (don't delete service as it's managed elsewhere)
    projectService_ = nullptr;
    context_ = nullptr;
    
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
    // Use QPointer to safely check if objects still exist
    QPointer<ProjectViewModel> viewModelPtr = viewModel_;
    QPointer<QTreeView> treeViewPtr = treeView;
    
    QObject::connect(treeView, &QTreeView::doubleClicked, [viewModelPtr, treeViewPtr](const QModelIndex &index) {
        if (viewModelPtr && treeViewPtr && viewModelPtr->isDirectory(index)) {
            QString path = viewModelPtr->getFilePath(index);
            viewModelPtr->setRootPath(path);
            if (treeViewPtr) {
                treeViewPtr->setRootIndex(viewModelPtr->rootIndex());
            }
        }
    });
    
    // Connect project opened signal to update tree view
    QPointer<ProjectPlugin> pluginPtr = this;
    QObject::connect(projectService_, &IProjectService::projectOpened, [pluginPtr, viewModelPtr, treeViewPtr]() {
        if (pluginPtr && viewModelPtr && treeViewPtr && pluginPtr->projectService_) {
            viewModelPtr->setRootPath(pluginPtr->projectService_->projectRoot());
            if (treeViewPtr) {
                treeViewPtr->setRootIndex(viewModelPtr->rootIndex());
            }
        }
    });
    
    layout->addWidget(treeView);
    fileBrowserWidget_->setLayout(layout);

    qDebug() << "ProjectPlugin loaded successfully";
    return true;
}

bool ProjectPlugin::unload() {
    // Disconnect all signals first
    if (projectService_) {
        QObject::disconnect(projectService_, nullptr, this, nullptr);
    }
    
    // CRITICAL: fileBrowserWidget_ ownership issue
    // Once fileBrowserWidget_ is added to DockWidget via dock->setWidget(),
    // the dock becomes its parent and owns it. ProjectPlugin should NOT delete it.
    // We only need to clear the model and disconnect signals, then release the pointer.
    if (fileBrowserWidget_) {
        QWidget *parent = fileBrowserWidget_->parentWidget();
        if (parent) {
            // Widget has a parent (likely DockWidget), so it's owned by the parent
            // We should NOT delete it, just clean up our references
            QDockWidget *dock = qobject_cast<QDockWidget *>(parent);
            if (dock) {
                // Clear model from tree view before widget is destroyed by dock
                QTreeView *treeView = fileBrowserWidget_->findChild<QTreeView *>();
                if (treeView) {
                    treeView->setModel(nullptr); // Important: clear model before widget destruction
                    treeView->disconnect();
                }
                
                // Disconnect signals from widget
                fileBrowserWidget_->disconnect();
                
                // Remove widget from dock (dock will handle deletion)
                dock->setWidget(nullptr);
                qDebug() << "ProjectPlugin::unload: Released fileBrowserWidget ownership to DockWidget";
            } else {
                // Widget has a different parent, still not ours to delete
                qDebug() << "ProjectPlugin::unload: fileBrowserWidget has parent, releasing ownership";
            }
        } else {
            // Widget has no parent, we own it and should delete it
            QTreeView *treeView = fileBrowserWidget_->findChild<QTreeView *>();
            if (treeView) {
                treeView->setModel(nullptr);
                treeView->disconnect();
            }
            fileBrowserWidget_->disconnect();
            fileBrowserWidget_->deleteLater();
            qDebug() << "ProjectPlugin::unload: Deleted fileBrowserWidget (no parent)";
        }
        
        // Always clear our pointer - widget is now owned by dock or deleted
        fileBrowserWidget_ = nullptr;
    }
    
    // Clean up ViewModel (this will also clean up QFileSystemModel)
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
