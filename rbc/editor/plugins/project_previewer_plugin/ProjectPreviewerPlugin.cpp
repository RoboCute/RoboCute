#include "ProjectPreviewerPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"

#include <QQmlEngine>
#include <QDebug>
#include <QDir>

namespace rbc {

// ============================================================================
// ProjectPreviewerViewModel Implementation
// ============================================================================

ProjectPreviewerViewModel::ProjectPreviewerViewModel(IProjectService *projectService, QObject *parent)
    : ViewModelBase(parent), projectService_(projectService) {

    if (!projectService_) {
        qWarning() << "ProjectPreviewerViewModel: projectService is null";
        return;
    }

    // Create and configure QFileSystemModel
    fileSystemModel_ = new QFileSystemModel(this);
    setupFilters();

    // Connect to service signals
    QObject::connect(projectService_, &IProjectService::projectOpened,
            this, &ProjectPreviewerViewModel::onProjectOpened);
    QObject::connect(projectService_, &IProjectService::projectClosed,
            this, &ProjectPreviewerViewModel::onProjectClosed);
    QObject::connect(projectService_, &IProjectService::projectInfoChanged,
            this, &ProjectPreviewerViewModel::onProjectInfoChanged);

    // Initialize root path if project is already open
    if (projectService_->isOpen()) {
        updateRootPath();
    }
}

ProjectPreviewerViewModel::~ProjectPreviewerViewModel() = default;

QString ProjectPreviewerViewModel::projectRoot() const {
    return projectService_ ? projectService_->projectRoot() : QString();
}

QModelIndex ProjectPreviewerViewModel::rootIndex() const {
    if (!fileSystemModel_ || currentRootPath_.isEmpty()) {
        return QModelIndex();
    }
    // Return the index for the root path
    // Note: QFileSystemModel::index() with a path returns the index for that path
    return fileSystemModel_->index(currentRootPath_);
}

void ProjectPreviewerViewModel::setFilter(const QString &filter) {
    if (filter_ != filter) {
        filter_ = filter;
        setupFilters();
        emit filterChanged();
    }
}

QString ProjectPreviewerViewModel::getFilePath(const QModelIndex &index) const {
    if (!fileSystemModel_ || !index.isValid()) {
        return QString();
    }
    return fileSystemModel_->filePath(index);
}

bool ProjectPreviewerViewModel::isDirectory(const QModelIndex &index) const {
    if (!fileSystemModel_ || !index.isValid()) {
        return false;
    }
    return fileSystemModel_->isDir(index);
}

QString ProjectPreviewerViewModel::getFileName(const QModelIndex &index) const {
    if (!fileSystemModel_ || !index.isValid()) {
        return QString();
    }
    return fileSystemModel_->fileName(index);
}

void ProjectPreviewerViewModel::setRootPath(const QString &path) {
    if (!fileSystemModel_ || path.isEmpty()) {
        return;
    }
    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "ProjectPreviewerViewModel::setRootPath: path does not exist:" << path;
        return;
    }
    currentRootPath_ = QDir::cleanPath(dir.absolutePath());
    fileSystemModel_->setRootPath(currentRootPath_);
    emit rootIndexChanged();
}

void ProjectPreviewerViewModel::refresh() {
    if (fileSystemModel_ && !currentRootPath_.isEmpty()) {
        fileSystemModel_->setRootPath(QString());  // Clear
        fileSystemModel_->setRootPath(currentRootPath_);  // Reset
    }
}

void ProjectPreviewerViewModel::navigateUp() {
    if (currentRootPath_.isEmpty() || !projectService_) {
        return;
    }
    
    const QString projectRoot = projectService_->projectRoot();
    if (currentRootPath_ == projectRoot) {
        return;  // Already at project root
    }
    
    QDir dir(currentRootPath_);
    if (dir.cdUp()) {
        QString parentPath = dir.absolutePath();
        // Don't navigate above project root
        if (parentPath.startsWith(projectRoot, Qt::CaseInsensitive) || parentPath == projectRoot) {
            setRootPath(parentPath);
        } else {
            setRootPath(projectRoot);
        }
    }
}

bool ProjectPreviewerViewModel::canNavigateUp() const {
    if (currentRootPath_.isEmpty() || !projectService_) {
        return false;
    }
    const QString projectRoot = projectService_->projectRoot();
    return currentRootPath_ != projectRoot;
}

void ProjectPreviewerViewModel::onProjectOpened() {
    updateRootPath();
}

void ProjectPreviewerViewModel::onProjectClosed() {
    currentRootPath_.clear();
    if (fileSystemModel_) {
        fileSystemModel_->setRootPath(QString());
    }
    emit rootIndexChanged();
    emit projectRootChanged();
}

void ProjectPreviewerViewModel::onProjectInfoChanged() {
    updateRootPath();
}

void ProjectPreviewerViewModel::updateRootPath() {
    if (!projectService_ || !projectService_->isOpen()) {
        return;
    }

    const QString root = projectService_->projectRoot();
    if (root != currentRootPath_) {
        setRootPath(root);
        emit projectRootChanged();
    }
}

void ProjectPreviewerViewModel::setupFilters() {
    if (!fileSystemModel_) {
        return;
    }

    // Configure filters based on project structure
    QStringList nameFilters;
    
    if (filter_ == "*" || filter_.isEmpty()) {
        // Show all files
        nameFilters << "*";
    } else {
        // Parse filter string (e.g., "*.rbcgraph,*.rbcscene")
        nameFilters = filter_.split(',', Qt::SkipEmptyParts);
        for (QString &filter : nameFilters) {
            filter = filter.trimmed();
        }
    }

    fileSystemModel_->setNameFilters(nameFilters);
    fileSystemModel_->setNameFilterDisables(false);  // Show matching files

    // Configure what to show
    fileSystemModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
}

QString ProjectPreviewerViewModel::getFilePathByRow(int row) const {
    const auto idx = indexForRow(row);
    return getFilePath(idx);
}

bool ProjectPreviewerViewModel::isDirectoryByRow(int row) const {
    const auto idx = indexForRow(row);
    return isDirectory(idx);
}

QString ProjectPreviewerViewModel::getFileNameByRow(int row) const {
    const auto idx = indexForRow(row);
    return getFileName(idx);
}

int ProjectPreviewerViewModel::rowCount() const {
    if (!fileSystemModel_ || currentRootPath_.isEmpty()) {
        return 0;
    }
    const auto rootIdx = fileSystemModel_->index(currentRootPath_);
    if (!rootIdx.isValid()) {
        return 0;
    }
    return fileSystemModel_->rowCount(rootIdx);
}

QModelIndex ProjectPreviewerViewModel::indexForRow(int row) const {
    if (!fileSystemModel_ || currentRootPath_.isEmpty() || row < 0) {
        return QModelIndex();
    }
    const auto rootIdx = fileSystemModel_->index(currentRootPath_);
    if (!rootIdx.isValid()) {
        return QModelIndex();
    }
    return fileSystemModel_->index(row, 0, rootIdx);
}

// ============================================================================
// ProjectPreviewerPlugin Implementation
// ============================================================================

ProjectPreviewerPlugin::ProjectPreviewerPlugin(QObject *parent)
    : IEditorPlugin(parent) {
    qDebug() << "ProjectPreviewerPlugin created";
}

ProjectPreviewerPlugin::~ProjectPreviewerPlugin() {
    qDebug() << "ProjectPreviewerPlugin destroyed";
}

bool ProjectPreviewerPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ProjectPreviewerPlugin::load: context is null";
        return false;
    }

    context_ = context;

    // Get ProjectService from PluginManager
    projectService_ = context->getService<IProjectService>();

    if (!projectService_) {
        qWarning() << "ProjectPreviewerPlugin::load: ProjectService should be registered before ProjectPreviewerPlugin load";
        return false;
    }

    // Create ViewModel
    viewModel_ = new ProjectPreviewerViewModel(projectService_, this);

    qDebug() << "ProjectPreviewerPlugin loaded successfully";
    return true;
}

bool ProjectPreviewerPlugin::unload() {
    if (viewModel_) {
        viewModel_->deleteLater();
        viewModel_ = nullptr;
    }

    // Don't delete projectService_ as it might be used by other plugins
    projectService_ = nullptr;
    context_ = nullptr;

    qDebug() << "ProjectPreviewerPlugin unloaded";
    return true;
}

bool ProjectPreviewerPlugin::reload() {
    qDebug() << "ProjectPreviewerPlugin reloading...";

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(context_)) {
        return false;
    }

    qDebug() << "ProjectPreviewerPlugin reloaded";
    return true;
}

QList<ViewContribution> ProjectPreviewerPlugin::view_contributions() const {
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

void ProjectPreviewerPlugin::register_view_models(QQmlEngine *engine) {
    if (!engine) {
        qWarning() << "ProjectPreviewerPlugin::register_view_models: engine is null";
        return;
    }

    // Register ProjectPreviewerViewModel as QML type
    qmlRegisterType<ProjectPreviewerViewModel>("RoboCute.ProjectPreviewer", 1, 0, "ProjectPreviewerViewModel");

    qDebug() << "ProjectPreviewerPlugin: ViewModels registered";
}

QObject *ProjectPreviewerPlugin::getViewModel(const QString &viewId) {
    if (viewId == "project_previewer" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

// Extern "C" Interface
IEditorPlugin *createPlugin() {
    // let upper parent manage the life cycle
    return static_cast<IEditorPlugin *>(new ProjectPreviewerPlugin());
}

}// namespace rbc
