#include "ProjectPlugin.h"

namespace rbc {

// ============================================================================
// ProjectViewModel Implementation
// ============================================================================

ProjectViewModel::ProjectViewModel(IProjectService *projectService, QObject *parent)
    : ViewModelBase(parent), projectService_(projectService) {

    if (!projectService_) {
        qWarning() << "ProjectViewModel: projectService is null";
        return;
    }

    // Create and configure QFileSystemModel
    fileSystemModel_ = new QFileSystemModel(this);
    setupFilters();

    // Connect to service signals
    QObject::connect(projectService_, &IProjectService::projectOpened,
                     this, &ProjectViewModel::onProjectOpened);
    QObject::connect(projectService_, &IProjectService::projectClosed,
                     this, &ProjectViewModel::onProjectClosed);
    QObject::connect(projectService_, &IProjectService::projectInfoChanged,
                     this, &ProjectViewModel::onProjectInfoChanged);

    // Initialize root path if project is already open
    if (projectService_->isOpen()) {
        updateRootPath();
    }
}

ProjectViewModel::~ProjectViewModel() = default;

QString ProjectViewModel::projectRoot() const {
    return projectService_ ? projectService_->projectRoot() : QString();
}

QModelIndex ProjectViewModel::rootIndex() const {
    if (!fileSystemModel_ || currentRootPath_.isEmpty()) {
        return QModelIndex();
    }
    // Return the index for the root path
    // Note: QFileSystemModel::index() with a path returns the index for that path
    return fileSystemModel_->index(currentRootPath_);
}

void ProjectViewModel::setFilter(const QString &filter) {
    if (filter_ != filter) {
        filter_ = filter;
        setupFilters();
        emit filterChanged();
    }
}

QString ProjectViewModel::getFilePath(const QModelIndex &index) const {
    if (!fileSystemModel_ || !index.isValid()) {
        return QString();
    }
    return fileSystemModel_->filePath(index);
}

bool ProjectViewModel::isDirectory(const QModelIndex &index) const {
    if (!fileSystemModel_ || !index.isValid()) {
        return false;
    }
    return fileSystemModel_->isDir(index);
}

QString ProjectViewModel::getFileName(const QModelIndex &index) const {
    if (!fileSystemModel_ || !index.isValid()) {
        return QString();
    }
    return fileSystemModel_->fileName(index);
}

void ProjectViewModel::setRootPath(const QString &path) {
    if (!fileSystemModel_ || path.isEmpty()) {
        return;
    }
    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "ProjectViewModel::setRootPath: path does not exist:" << path;
        return;
    }
    currentRootPath_ = QDir::cleanPath(dir.absolutePath());
    fileSystemModel_->setRootPath(currentRootPath_);
    emit rootIndexChanged();
}

void ProjectViewModel::refresh() {
    if (fileSystemModel_ && !currentRootPath_.isEmpty()) {
        fileSystemModel_->setRootPath(QString());       // Clear
        fileSystemModel_->setRootPath(currentRootPath_);// Reset
    }
}

void ProjectViewModel::navigateUp() {
    if (currentRootPath_.isEmpty() || !projectService_) {
        return;
    }

    const QString projectRoot = projectService_->projectRoot();
    if (currentRootPath_ == projectRoot) {
        return;// Already at project root
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

bool ProjectViewModel::canNavigateUp() const {
    if (currentRootPath_.isEmpty() || !projectService_) {
        return false;
    }
    const QString projectRoot = projectService_->projectRoot();
    return currentRootPath_ != projectRoot;
}

void ProjectViewModel::onProjectOpened() {
    updateRootPath();
}

void ProjectViewModel::onProjectClosed() {
    currentRootPath_.clear();
    if (fileSystemModel_) {
        fileSystemModel_->setRootPath(QString());
    }
    emit rootIndexChanged();
    emit projectRootChanged();
}

void ProjectViewModel::onProjectInfoChanged() {
    updateRootPath();
}

void ProjectViewModel::updateRootPath() {
    if (!projectService_ || !projectService_->isOpen()) {
        return;
    }

    const QString root = projectService_->projectRoot();
    if (root != currentRootPath_) {
        setRootPath(root);
        emit projectRootChanged();
    }
}

void ProjectViewModel::setupFilters() {
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
    fileSystemModel_->setNameFilterDisables(false);// Show matching files

    // Configure what to show
    fileSystemModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
}

QString ProjectViewModel::getFilePathByRow(int row) const {
    const auto idx = indexForRow(row);
    return getFilePath(idx);
}

bool ProjectViewModel::isDirectoryByRow(int row) const {
    const auto idx = indexForRow(row);
    return isDirectory(idx);
}

QString ProjectViewModel::getFileNameByRow(int row) const {
    const auto idx = indexForRow(row);
    return getFileName(idx);
}

int ProjectViewModel::rowCount() const {
    if (!fileSystemModel_ || currentRootPath_.isEmpty()) {
        return 0;
    }
    const auto rootIdx = fileSystemModel_->index(currentRootPath_);
    if (!rootIdx.isValid()) {
        return 0;
    }
    return fileSystemModel_->rowCount(rootIdx);
}

QModelIndex ProjectViewModel::indexForRow(int row) const {
    if (!fileSystemModel_ || currentRootPath_.isEmpty() || row < 0) {
        return QModelIndex();
    }
    const auto rootIdx = fileSystemModel_->index(currentRootPath_);
    if (!rootIdx.isValid()) {
        return QModelIndex();
    }
    return fileSystemModel_->index(row, 0, rootIdx);
}

}// namespace rbc