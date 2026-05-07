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
    if (_project_opened_connection) {
        QObject::disconnect(_project_opened_connection);
        _project_opened_connection = {};
    }
    if (_tree_view_double_click_connection) {
        QObject::disconnect(_tree_view_double_click_connection);
        _tree_view_double_click_connection = {};
    }
    if (_project_closing_connection) {
        QObject::disconnect(_project_closing_connection);
        _project_closing_connection = {};
    }

    // Save cache before destruction
    if (_project_service && _project_service->isOpen()) {
        saveProjectCache();
    }

    if (_file_browser_widget || _view_model) {
        qWarning() << "ProjectPlugin::~ProjectPlugin: unload() was not called before destruction!";
        if (_view_model) {
            delete _view_model;
            _view_model = nullptr;
        }
        // _file_browser_widget uses QPointer, check if already deleted
        if (_file_browser_widget) {
            delete _file_browser_widget.data();
            // QPointer auto-becomes nullptr
        }
    }

    _project_service = nullptr;
    _context = nullptr;

    qDebug() << "ProjectPlugin destroyed";
}

bool ProjectPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ProjectPlugin::load: context is null";
        return false;
    }

    _context = context;

    // Get ProjectService from PluginManager (using interface type)
    _project_service = context->getService<IProjectService>();

    if (!_project_service) {
        qWarning() << "ProjectPlugin::load: ProjectService should be registered before ProjectPlugin load";
        return false;
    }

    // Load project cache
    loadProjectCache();

    // Create ViewModel first (needed for signal connections)
    _view_model = new ProjectViewModel(_project_service, this);

    // Try to open last project if available
    QString lastProject = getLastOpenedProject();
    if (!lastProject.isEmpty() && QFileInfo::exists(lastProject)) {
        ProjectOpenOptions options;
        options.loadUserPreferences = true;
        options.loadEditorSession = true;
        
        if (_project_service->openProject(lastProject, options)) {
            qDebug() << "ProjectPlugin: Auto-opened last project:" << lastProject;
            // ViewModel will be updated via projectOpened signal connection
        } else {
            qWarning() << "ProjectPlugin: Failed to auto-open last project:" << _project_service->lastError();
        }
    }
    
    // If no project is open, show project list
    if (!_project_service->isOpen()) {
        _view_model->setProjectListMode(true);
        _view_model->setRecentProjects(getRecentProjects());
    }

    // Create file browser widget
    _file_browser_widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(_file_browser_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    QTreeView *treeView = new QTreeView(_file_browser_widget);
    treeView->setModel(_view_model->fileSystemModel());
    treeView->setRootIndex(_view_model->rootIndex());
    treeView->setHeaderHidden(false);
    treeView->setAlternatingRowColors(true);
    treeView->setAnimated(true);
    treeView->setIndentation(20);
    treeView->setSortingEnabled(true);

    // Apply unified style using StyleManager
    IStyleManager *styleManager = context->getService<IStyleManager>();
    if (styleManager) {
        if (!styleManager->applyStylePreset(treeView, "FileTree")) {
            qWarning() << "ProjectPlugin: Failed to apply FileTree style preset";
        }
    } else {
        qWarning() << "ProjectPlugin: StyleManager not found, FileTree will use default style";
    }

    QPointer<ProjectViewModel> viewModelPtr = _view_model;
    QPointer<QTreeView> treeViewPtr = treeView;
    _tree_view_double_click_connection = QObject::connect(
        treeView, &QTreeView::doubleClicked,
        this,// context object - ensures connection auto-disconnects when plugin is deleted
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
    _project_opened_connection = QObject::connect(
        _project_service, &IProjectService::projectOpened,
        this,// context object - this is key! Without it disconnect(sender, nullptr, this, nullptr) cannot disconnect lambda connections
        [pluginPtr, viewModelPtr, treeViewPtr]() {
            if (pluginPtr && viewModelPtr && treeViewPtr && pluginPtr->_project_service) {
                // Switch to tree mode when project opens
                viewModelPtr->setProjectListMode(false);
                viewModelPtr->setRootPath(pluginPtr->_project_service->projectRoot());
                if (treeViewPtr) {
                    treeViewPtr->setRootIndex(viewModelPtr->rootIndex());
                }
                // Add to cache
                pluginPtr->addProjectToCache(pluginPtr->_project_service->projectRoot());
            }
        });
    
    _project_closing_connection = QObject::connect(
        _project_service, &IProjectService::projectClosing,
        this,
        [pluginPtr]() {
            if (pluginPtr && pluginPtr->_project_service) {
                // Save cache when project is closing
                pluginPtr->saveProjectCache();
            }
        });

    layout->addWidget(treeView);
    _file_browser_widget->setLayout(layout);

    // Register NativeViewContribution for file browser
    NativeViewContribution fileBrowserContrib;
    fileBrowserContrib.viewId = "project_file_browser";
    fileBrowserContrib.title = "Project Files";
    fileBrowserContrib.dockArea = "Left";
    fileBrowserContrib.closable = true;
    fileBrowserContrib.movable = true;
    fileBrowserContrib.floatable = true;
    fileBrowserContrib.isExternalManaged = true; // Lifecycle managed by Plugin
    
    _registered_contributions.append(fileBrowserContrib);

    qDebug() << "ProjectPlugin loaded successfully";
    return true;
}

bool ProjectPlugin::unload() {
    qDebug() << "ProjectPlugin::unload: Starting unload...";

    // 1. Explicitly disconnect all saved connection handles
    //    This is necessary because we need to disconnect before deleting _view_model,
    //    otherwise during the window between _view_model deletion and plugin deletion,
    //    if _project_service emits signals, lambda will be called
    if (_project_opened_connection) {
        QObject::disconnect(_project_opened_connection);
        _project_opened_connection = {};
        qDebug() << "ProjectPlugin::unload: Disconnected projectOpenedConnection";
    }
    if (_tree_view_double_click_connection) {
        QObject::disconnect(_tree_view_double_click_connection);
        _tree_view_double_click_connection = {};
        qDebug() << "ProjectPlugin::unload: Disconnected treeViewDoubleClickConnection";
    }

    // 2. Disconnect other possible signal connections (using receiver matching)
    if (_project_closing_connection) {
        QObject::disconnect(_project_closing_connection);
        _project_closing_connection = {};
        qDebug() << "ProjectPlugin::unload: Disconnected projectClosingConnection";
    }
    
    // Save cache before unloading
    if (_project_service && _project_service->isOpen()) {
        saveProjectCache();
    }
    
    if (_project_service) {
        QObject::disconnect(_project_service, nullptr, this, nullptr);
    }

    // 2. Clean up _file_browser_widget
    // Use QPointer to check if widget still exists
    // If Qt already deleted widget (e.g., via parent-child mechanism), QPointer becomes nullptr
    if (_file_browser_widget) {
        // Clean up tree view model reference
        QTreeView *treeView = _file_browser_widget->findChild<QTreeView *>();
        if (treeView) {
            treeView->setModel(nullptr);
        }

        // Delete widget (if WindowManager correctly called cleanup(), widget should have no parent)
        qDebug() << "ProjectPlugin::unload: Deleting fileBrowserWidget";
        delete _file_browser_widget.data();
        // QPointer auto-becomes nullptr, no manual setting needed
    } else {
        qDebug() << "ProjectPlugin::unload: fileBrowserWidget already deleted";
    }

    // 3. Clean up ViewModel
    if (_view_model) {
        delete _view_model;
        _view_model = nullptr;
        qDebug() << "ProjectPlugin::unload: Deleted viewModel";
    }

    // 4. Clean up NativeViewContributions
    _registered_contributions.clear();

    // 5. Clean up references (do not delete service, it is managed elsewhere)
    _project_service = nullptr;
    _context = nullptr;

    qDebug() << "ProjectPlugin::unload: Unload completed";
    return true;
}

bool ProjectPlugin::reload() {
    qDebug() << "ProjectPlugin reloading...";

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(_context)) {
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
    return _registered_contributions;
}

QList<MenuContribution> ProjectPlugin::menu_contributions() const {
    MenuContribution menuItem;
    menuItem.menuPath = "File/Open Project";// Full path including action text
    menuItem.actionText = "Open Project";
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
    if (viewId == "project_previewer" && _view_model) {
        return _view_model;
    }
    return nullptr;
}

QWidget *ProjectPlugin::getNativeWidget(const QString &viewId) {
    if (viewId == "project_file_browser") {
        return _file_browser_widget.data();  // QPointer::data() returns raw pointer
    }
    return nullptr;
}

void ProjectPlugin::onOpenProjectTriggered() {
    if (!_project_service) {
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
        "Select Project Folder",
        defaultDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (projectPath.isEmpty()) {
        return;// User cancelled
    }

    // Open project
    ProjectOpenOptions options;
    options.loadUserPreferences = true;
    options.loadEditorSession = true;

    if (!_project_service->openProject(projectPath, options)) {
        qWarning() << "ProjectPlugin::onOpenProjectTriggered: Failed to open project:"
                   << _project_service->lastError();
        return;
    }

    qDebug() << "ProjectPlugin: Project opened successfully:" << projectPath;

    // Update file browser widget if it exists
    if (_file_browser_widget) {
        if (_view_model) {
            _view_model->setRootPath(projectPath);
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
    if (_view_model) {
        _view_model->setRecentProjects(recentProjects);
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
    if (_project_service && _project_service->isOpen()) {
        obj.insert("last_opened_project", _project_service->projectRoot());
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
    if (_view_model) {
        _view_model->setRecentProjects(recentProjects);
    }
    
    // Save immediately
    saveProjectCache();
}

QStringList ProjectPlugin::getRecentProjects() const {
    if (_view_model) {
        return _view_model->recentProjects();
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

// Export factory function (new design)
// PluginManager manages plugin lifecycles through factories
IPluginFactory *createPluginFactory() {
    return new PluginFactory<ProjectPlugin>();
}

}// namespace rbc
