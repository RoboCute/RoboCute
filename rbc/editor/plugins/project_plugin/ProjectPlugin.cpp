#include "ProjectPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"
#include "RBCEditorRuntime/services/IStyleManager.h"

#include <QQmlEngine>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QDockWidget>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QListWidget>
#include <QCoreApplication>
#include <QDateTime>

namespace rbc {

// ============================================================================
// ProjectPlugin Implementation
// ============================================================================

ProjectPlugin::ProjectPlugin(QObject *parent)
    : IEditorPlugin(parent) {
    qDebug() << "ProjectPlugin created";
}

ProjectPlugin::~ProjectPlugin() {
    if (projectOpenedConnection_) {
        QObject::disconnect(projectOpenedConnection_);
        projectOpenedConnection_ = {};
    }
    if (treeViewDoubleClickConnection_) {
        QObject::disconnect(treeViewDoubleClickConnection_);
        treeViewDoubleClickConnection_ = {};
    }
    if (projectClosingConnection_) {
        QObject::disconnect(projectClosingConnection_);
        projectClosingConnection_ = {};
    }

    // Save cache before destruction
    if (projectService_ && projectService_->isOpen()) {
        saveProjectCache();
    }

    if (fileBrowserWidget_ || viewModel_) {
        qWarning() << "ProjectPlugin::~ProjectPlugin: unload() was not called before destruction!";
        if (viewModel_) {
            delete viewModel_;
            viewModel_ = nullptr;
        }
        // fileBrowserWidget_ 使用 QPointer，检查是否已被删除
        if (fileBrowserWidget_) {
            delete fileBrowserWidget_.data();
            // QPointer 会自动变成 nullptr
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

    // Load project cache
    loadProjectCache();

    // Create ViewModel first (needed for signal connections)
    viewModel_ = new ProjectViewModel(projectService_, this);

    // Try to open last project if available
    QString lastProject = getLastOpenedProject();
    if (!lastProject.isEmpty() && QFileInfo::exists(lastProject)) {
        ProjectOpenOptions options;
        options.loadUserPreferences = true;
        options.loadEditorSession = true;
        
        if (projectService_->openProject(lastProject, options)) {
            qDebug() << "ProjectPlugin: Auto-opened last project:" << lastProject;
            // ViewModel will be updated via projectOpened signal connection
        } else {
            qWarning() << "ProjectPlugin: Failed to auto-open last project:" << projectService_->lastError();
        }
    }
    
    // If no project is open, show project list
    if (!projectService_->isOpen()) {
        viewModel_->setProjectListMode(true);
        viewModel_->setRecentProjects(getRecentProjects());
    }

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

    // Apply unified style using StyleManager
    IStyleManager *styleManager = context->getService<IStyleManager>();
    if (styleManager) {
        styleManager->applyStylePreset(treeView, "FileTree");
        qDebug() << "ProjectPlugin: Applied FileTree style preset";
    } else {
        qWarning() << "ProjectPlugin: StyleManager not found, FileTree will use default style";
    }

    QPointer<ProjectViewModel> viewModelPtr = viewModel_;
    QPointer<QTreeView> treeViewPtr = treeView;
    treeViewDoubleClickConnection_ = QObject::connect(
        treeView, &QTreeView::doubleClicked,
        this,// context 对象 - 确保 plugin 删除时连接自动断开
        [viewModelPtr, treeViewPtr](const QModelIndex &index) {
            if (viewModelPtr && treeViewPtr && viewModelPtr->isDirectory(index)) {
                QString path = viewModelPtr->getFilePath(index);
                viewModelPtr->setRootPath(path);
                if (treeViewPtr) {
                    treeViewPtr->setRootIndex(viewModelPtr->rootIndex());
                }
            }
        });

    QPointer<ProjectPlugin> pluginPtr = this;
    projectOpenedConnection_ = QObject::connect(
        projectService_, &IProjectService::projectOpened,
        this,// context 对象 - 这是关键！没有它 disconnect(sender, nullptr, this, nullptr) 无法断开 lambda 连接
        [pluginPtr, viewModelPtr, treeViewPtr]() {
            if (pluginPtr && viewModelPtr && treeViewPtr && pluginPtr->projectService_) {
                // Switch to tree mode when project opens
                viewModelPtr->setProjectListMode(false);
                viewModelPtr->setRootPath(pluginPtr->projectService_->projectRoot());
                if (treeViewPtr) {
                    treeViewPtr->setRootIndex(viewModelPtr->rootIndex());
                }
                // Add to cache
                pluginPtr->addProjectToCache(pluginPtr->projectService_->projectRoot());
            }
        });
    
    projectClosingConnection_ = QObject::connect(
        projectService_, &IProjectService::projectClosing,
        this,
        [pluginPtr]() {
            if (pluginPtr && pluginPtr->projectService_) {
                // Save cache when project is closing
                pluginPtr->saveProjectCache();
            }
        });

    layout->addWidget(treeView);
    fileBrowserWidget_->setLayout(layout);

    // Register NativeViewContribution for file browser
    NativeViewContribution fileBrowserContrib;
    fileBrowserContrib.viewId = "project_file_browser";
    fileBrowserContrib.title = "Project Files";
    fileBrowserContrib.dockArea = "Left";
    fileBrowserContrib.closable = true;
    fileBrowserContrib.movable = true;
    fileBrowserContrib.floatable = true;
    fileBrowserContrib.isExternalManaged = true; // 由 Plugin 管理生命周期
    
    registeredContributions_.append(fileBrowserContrib);

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
    if (projectClosingConnection_) {
        QObject::disconnect(projectClosingConnection_);
        projectClosingConnection_ = {};
        qDebug() << "ProjectPlugin::unload: Disconnected projectClosingConnection";
    }
    
    // Save cache before unloading
    if (projectService_ && projectService_->isOpen()) {
        saveProjectCache();
    }
    
    if (projectService_) {
        QObject::disconnect(projectService_, nullptr, this, nullptr);
    }

    // 2. 清理 fileBrowserWidget_
    // 使用 QPointer 检查 widget 是否仍然存在
    // 如果 Qt 已经删除了 widget（例如通过 parent-child 机制），QPointer 会变成 nullptr
    if (fileBrowserWidget_) {
        // 清理 tree view 的 model 引用
        QTreeView *treeView = fileBrowserWidget_->findChild<QTreeView *>();
        if (treeView) {
            treeView->setModel(nullptr);
        }

        // 删除 widget（如果 WindowManager 正确调用了 cleanup()，widget 应该没有 parent）
        qDebug() << "ProjectPlugin::unload: Deleting fileBrowserWidget";
        delete fileBrowserWidget_.data();
        // QPointer 会自动变成 nullptr，无需手动设置
    } else {
        qDebug() << "ProjectPlugin::unload: fileBrowserWidget already deleted";
    }

    // 3. 清理 ViewModel
    if (viewModel_) {
        delete viewModel_;
        viewModel_ = nullptr;
        qDebug() << "ProjectPlugin::unload: Deleted viewModel";
    }

    // 4. 清理 NativeViewContributions
    registeredContributions_.clear();

    // 5. 清理引用（不删除 service，它由其他地方管理）
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

QList<NativeViewContribution> ProjectPlugin::native_view_contributions() const {
    return registeredContributions_;
}

QList<MenuContribution> ProjectPlugin::menu_contributions() const {
    MenuContribution menuItem;
    menuItem.menuPath = "File/打开Project";// Full path including action text
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

QWidget *ProjectPlugin::getNativeWidget(const QString &viewId) {
    if (viewId == "project_file_browser") {
        return fileBrowserWidget_.data();  // QPointer::data() 返回原始指针
    }
    return nullptr;
}

void ProjectPlugin::onOpenProjectTriggered() {
    if (!projectService_) {
        qWarning() << "ProjectPlugin::onOpenProjectTriggered: projectService is null";
        return;
    }

    // Get last opened project directory as default
    QString defaultDir;
    QString lastProject = getLastOpenedProject();
    if (!lastProject.isEmpty()) {
        QFileInfo fi(lastProject);
        if (fi.exists()) {
            defaultDir = fi.absolutePath();
        }
    }

    // Open file dialog to select project folder
    QString projectPath = QFileDialog::getExistingDirectory(
        nullptr,
        "选择Project文件夹",
        defaultDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (projectPath.isEmpty()) {
        return;// User cancelled
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

QString ProjectPlugin::getWorkDir() const {
    // Use QStandardPaths to get application data directory
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        // Fallback to application directory
        appDataPath = QCoreApplication::applicationDirPath();
    }
    return appDataPath;
}

QString ProjectPlugin::getCacheFilePath() const {
    QDir workDir(getWorkDir());
    if (!workDir.exists()) {
        workDir.mkpath(".");
    }
    return workDir.filePath("ProjectCache.json");
}

void ProjectPlugin::loadProjectCache() {
    QString cacheFile = getCacheFilePath();
    if (!QFileInfo::exists(cacheFile)) {
        qDebug() << "ProjectPlugin::loadProjectCache: Cache file does not exist:" << cacheFile;
        return;
    }

    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ProjectPlugin::loadProjectCache: Failed to open cache file:" << cacheFile;
        return;
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "ProjectPlugin::loadProjectCache: Failed to parse JSON:" << error.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QJsonArray recentArray = obj.value("recent_projects").toArray();
    QStringList recentProjects;
    for (const QJsonValue &value : recentArray) {
        QString path = value.toString();
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            recentProjects.append(path);
        }
    }
    
    // Store in viewModel if it exists
    if (viewModel_) {
        viewModel_->setRecentProjects(recentProjects);
    }
    
    qDebug() << "ProjectPlugin::loadProjectCache: Loaded" << recentProjects.size() << "recent projects";
}

void ProjectPlugin::saveProjectCache() {
    QStringList recentProjects = getRecentProjects();
    
    QJsonObject obj;
    QJsonArray recentArray;
    for (const QString &path : recentProjects) {
        recentArray.append(path);
    }
    obj.insert("recent_projects", recentArray);
    
    // Add last opened project
    if (projectService_ && projectService_->isOpen()) {
        obj.insert("last_opened_project", projectService_->projectRoot());
    } else {
        QString lastProject = getLastOpenedProject();
        if (!lastProject.isEmpty()) {
            obj.insert("last_opened_project", lastProject);
        }
    }
    
    obj.insert("saved_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QString cacheFile = getCacheFilePath();
    QFile file(cacheFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ProjectPlugin::saveProjectCache: Failed to open cache file for writing:" << cacheFile;
        return;
    }

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    qDebug() << "ProjectPlugin::saveProjectCache: Saved cache to:" << cacheFile;
}

void ProjectPlugin::addProjectToCache(const QString &projectPath) {
    if (projectPath.isEmpty()) {
        return;
    }

    QStringList recentProjects = getRecentProjects();
    
    // Remove if already exists
    recentProjects.removeAll(projectPath);
    
    // Add to front
    recentProjects.prepend(projectPath);
    
    // Limit to 20 recent projects
    while (recentProjects.size() > 20) {
        recentProjects.removeLast();
    }
    
    // Update viewModel
    if (viewModel_) {
        viewModel_->setRecentProjects(recentProjects);
    }
    
    // Save immediately
    saveProjectCache();
}

QStringList ProjectPlugin::getRecentProjects() const {
    if (viewModel_) {
        return viewModel_->recentProjects();
    }
    
    // Load from cache if viewModel not available
    QString cacheFile = getCacheFilePath();
    if (!QFileInfo::exists(cacheFile)) {
        return QStringList();
    }

    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringList();
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return QStringList();
    }

    QJsonObject obj = doc.object();
    QJsonArray recentArray = obj.value("recent_projects").toArray();
    QStringList recentProjects;
    for (const QJsonValue &value : recentArray) {
        QString path = value.toString();
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            recentProjects.append(path);
        }
    }
    
    return recentProjects;
}

QString ProjectPlugin::getLastOpenedProject() const {
    QString cacheFile = getCacheFilePath();
    if (!QFileInfo::exists(cacheFile)) {
        return QString();
    }

    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return QString();
    }

    QJsonObject obj = doc.object();
    QString lastProject = obj.value("last_opened_project").toString();
    
    // Verify it still exists
    if (!lastProject.isEmpty() && QFileInfo::exists(lastProject)) {
        return lastProject;
    }
    
    return QString();
}

// 导出工厂函数（新设计）
// PluginManager 通过工厂统一管理插件生命周期
IPluginFactory *createPluginFactory() {
    return new PluginFactory<ProjectPlugin>();
}

}// namespace rbc
