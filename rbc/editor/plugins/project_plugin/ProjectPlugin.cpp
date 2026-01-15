#include "ProjectPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"

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
    // 析构器保持简单，依赖 unload() 已经被调用
    // 如果 unload() 没有被调用，在这里做最小化清理
    
    // 首先断开所有连接（即使 unload 已经调用过，重复断开是安全的）
    // 这是关键的安全保障，防止析构过程中 service 发送信号
    if (projectOpenedConnection_) {
        QObject::disconnect(projectOpenedConnection_);
        projectOpenedConnection_ = {};
    }
    if (treeViewDoubleClickConnection_) {
        QObject::disconnect(treeViewDoubleClickConnection_);
        treeViewDoubleClickConnection_ = {};
    }
    
    if (fileBrowserWidget_ || viewModel_) {
        qWarning() << "ProjectPlugin::~ProjectPlugin: unload() was not called before destruction!";
        // 尝试清理，但可能不完整
        if (viewModel_) {
            delete viewModel_;
            viewModel_ = nullptr;
        }
        if (fileBrowserWidget_) {
            delete fileBrowserWidget_;
            fileBrowserWidget_ = nullptr;
        }
    }
    
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
    
    // 重要修复：保存连接句柄，以便在 unload 时显式断开
    // 同时使用 this 作为 context，这样当 plugin 被删除时连接也会自动断开
    treeViewDoubleClickConnection_ = QObject::connect(
        treeView, &QTreeView::doubleClicked, 
        this,  // context 对象 - 确保 plugin 删除时连接自动断开
        [viewModelPtr, treeViewPtr](const QModelIndex &index) {
            if (viewModelPtr && treeViewPtr && viewModelPtr->isDirectory(index)) {
                QString path = viewModelPtr->getFilePath(index);
                viewModelPtr->setRootPath(path);
                if (treeViewPtr) {
                    treeViewPtr->setRootIndex(viewModelPtr->rootIndex());
                }
            }
        });
    
    // Connect project opened signal to update tree view
    // 关键修复：
    // 1. 保存连接句柄到 projectOpenedConnection_
    // 2. 使用 this 作为 context 对象
    // 这样在 unload() 中可以通过句柄显式断开，
    // 即使 unload() 没有被调用，当 plugin 删除时连接也会自动断开
    QPointer<ProjectPlugin> pluginPtr = this;
    projectOpenedConnection_ = QObject::connect(
        projectService_, &IProjectService::projectOpened, 
        this,  // context 对象 - 这是关键！没有它 disconnect(sender, nullptr, this, nullptr) 无法断开 lambda 连接
        [pluginPtr, viewModelPtr, treeViewPtr]() {
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
    qDebug() << "ProjectPlugin::unload: Starting unload...";
    
    // 1. 显式断开所有保存的连接句柄
    //    这是必要的，因为我们需要在删除 viewModel_ 之前断开连接，
    //    否则在 viewModel_ 删除后到 plugin 删除前的窗口期内，
    //    如果 projectService_ 发送信号，lambda 会被调用
    if (projectOpenedConnection_) {
        QObject::disconnect(projectOpenedConnection_);
        projectOpenedConnection_ = {};
        qDebug() << "ProjectPlugin::unload: Disconnected projectOpenedConnection";
    }
    if (treeViewDoubleClickConnection_) {
        QObject::disconnect(treeViewDoubleClickConnection_);
        treeViewDoubleClickConnection_ = {};
        qDebug() << "ProjectPlugin::unload: Disconnected treeViewDoubleClickConnection";
    }
    
    // 2. 断开其他可能的信号连接（使用 receiver 匹配）
    if (projectService_) {
        QObject::disconnect(projectService_, nullptr, this, nullptr);
    }
    
    // 2. 清理 fileBrowserWidget_
    //    新范式：WindowManager.cleanup() 已经在 plugin unload 之前被调用
    //    它会从 dock 中移除外部 widget，所以这里 widget 应该没有 parent
    //    我们可以安全地删除它
    if (fileBrowserWidget_) {
        // 清理 tree view 的 model 引用
        QTreeView *treeView = fileBrowserWidget_->findChild<QTreeView *>();
        if (treeView) {
            treeView->setModel(nullptr);
        }
        
        // 删除 widget（如果 WindowManager 正确调用了 cleanup()，widget 应该没有 parent）
        delete fileBrowserWidget_;
        fileBrowserWidget_ = nullptr;
        qDebug() << "ProjectPlugin::unload: Deleted fileBrowserWidget";
    }
    
    // 3. 清理 ViewModel
    if (viewModel_) {
        delete viewModel_;
        viewModel_ = nullptr;
        qDebug() << "ProjectPlugin::unload: Deleted viewModel";
    }

    // 4. 清理引用（不删除 service，它由其他地方管理）
    projectService_ = nullptr;
    context_ = nullptr;

    qDebug() << "ProjectPlugin::unload: Unload completed";
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

// 导出工厂函数（新设计）
// PluginManager 通过工厂统一管理插件生命周期
IPluginFactory *createPluginFactory() {
    return new PluginFactory<ProjectPlugin>();
}

}// namespace rbc
