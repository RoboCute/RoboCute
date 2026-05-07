#include "ProjectPlugin.h"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>

namespace rbc {

// ============================================================================
// ProjectViewModel Implementation
// ============================================================================

ProjectViewModel::ProjectViewModel(IProjectService *projectService, QObject *parent)
    : ViewModelBase(parent), _project_service(projectService) {

    if (!_project_service) {
        qWarning() << "ProjectViewModel: projectService is null";
        return;
    }

    // Create and configure QFileSystemModel
    _file_system_model = new QFileSystemModel(this);
    setupFilters();

    // Connect to service signals
    QObject::connect(_project_service, &IProjectService::projectOpened,
                     this, &ProjectViewModel::onProjectOpened);
    QObject::connect(_project_service, &IProjectService::projectClosed,
                     this, &ProjectViewModel::onProjectClosed);
    QObject::connect(_project_service, &IProjectService::projectInfoChanged,
                     this, &ProjectViewModel::onProjectInfoChanged);

    // Initialize root path if project is already open
    if (_project_service->isOpen()) {
        updateRootPath();
    }
}

ProjectViewModel::~ProjectViewModel() {
    // Key: explicitly disconnect signal-slot connections with Service during destruction
    // Use correct syntax: QObject::disconnect(sender, signal, receiver, slot)
    // 
    // Why must we explicitly disconnect?
    // 1. Service lifetime is longer than ViewModel (Service deleted only when app is destroyed)
    // 2. If relying on Qt auto-cleanup, Service destructor will access deleted ViewModel
    // 3. Explicit disconnect is safe because this still exists
    
    if (_project_service) {
        // Disconnect all connections from _project_service to this
        // This is safe: both sender and receiver still exist
        QObject::disconnect(_project_service, nullptr, this, nullptr);
    }
    
    // Clear file system model root path to stop any background operations
    if (_file_system_model) {
        _file_system_model->setRootPath(QString());
        // _file_system_model will be automatically deleted as it's a child of this
    }
    
    // Clear references
    _project_service = nullptr;
    _file_system_model = nullptr;
}

QString ProjectViewModel::projectRoot() const {
    return _project_service ? _project_service->projectRoot() : QString();
}

QModelIndex ProjectViewModel::rootIndex() const {
    if (!_file_system_model || _current_root_path.isEmpty()) {
        return QModelIndex();
    }
    // Return the index for the root path
    // Note: QFileSystemModel::index() with a path returns the index for that path
    return _file_system_model->index(_current_root_path);
}

void ProjectViewModel::setFilter(const QString &filter) {
    if (_filter != filter) {
        _filter = filter;
        setupFilters();
        emit filterChanged();
    }
}

QString ProjectViewModel::getFilePath(const QModelIndex &index) const {
    if (!_file_system_model || !index.isValid()) {
        return QString();
    }
    return _file_system_model->filePath(index);
}

bool ProjectViewModel::isDirectory(const QModelIndex &index) const {
    if (!_file_system_model || !index.isValid()) {
        return false;
    }
    return _file_system_model->isDir(index);
}

QString ProjectViewModel::getFileName(const QModelIndex &index) const {
    if (!_file_system_model || !index.isValid()) {
        return QString();
    }
    return _file_system_model->fileName(index);
}

void ProjectViewModel::setRootPath(const QString &path) {
    if (!_file_system_model || path.isEmpty()) {
        return;
    }
    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "ProjectViewModel::setRootPath: path does not exist:" << path;
        return;
    }
    _current_root_path = QDir::cleanPath(dir.absolutePath());
    _file_system_model->setRootPath(_current_root_path);
    emit rootIndexChanged();
}

void ProjectViewModel::refresh() {
    if (_file_system_model && !_current_root_path.isEmpty()) {
        _file_system_model->setRootPath(QString());       // Clear
        _file_system_model->setRootPath(_current_root_path);// Reset
    }
}

void ProjectViewModel::navigateUp() {
    if (_current_root_path.isEmpty() || !_project_service) {
        return;
    }

    const QString projectRoot = _project_service->projectRoot();
    if (_current_root_path == projectRoot) {
        return;// Already at project root
    }

    QDir dir(_current_root_path);
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
    if (_current_root_path.isEmpty() || !_project_service) {
        return false;
    }
    const QString projectRoot = _project_service->projectRoot();
    return _current_root_path != projectRoot;
}

void ProjectViewModel::onProjectOpened() {
    updateRootPath();
}

void ProjectViewModel::onProjectClosed() {
    _current_root_path.clear();
    if (_file_system_model) {
        _file_system_model->setRootPath(QString());
    }
    emit rootIndexChanged();
    emit projectRootChanged();
}

void ProjectViewModel::onProjectInfoChanged() {
    updateRootPath();
}

void ProjectViewModel::updateRootPath() {
    if (!_project_service || !_project_service->isOpen()) {
        return;
    }

    const QString root = _project_service->projectRoot();
    if (root != _current_root_path) {
        setRootPath(root);
        emit projectRootChanged();
    }
}

void ProjectViewModel::setupFilters() {
    if (!_file_system_model) {
        return;
    }

    // Configure filters based on project structure
    QStringList nameFilters;

    if (_filter == "*" || _filter.isEmpty()) {
        // Show all files
        nameFilters << "*";
    } else {
        // Parse filter string (e.g., "*.rbcgraph,*.rbcscene")
        nameFilters = _filter.split(',', Qt::SkipEmptyParts);
        for (QString &filter : nameFilters) {
            filter = filter.trimmed();
        }
    }

    _file_system_model->setNameFilters(nameFilters);
    _file_system_model->setNameFilterDisables(false);// Show matching files

    // Configure what to show
    _file_system_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
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
    if (!_file_system_model || _current_root_path.isEmpty()) {
        return 0;
    }
    const auto rootIdx = _file_system_model->index(_current_root_path);
    if (!rootIdx.isValid()) {
        return 0;
    }
    return _file_system_model->rowCount(rootIdx);
}

QModelIndex ProjectViewModel::indexForRow(int row) const {
    if (!_file_system_model || _current_root_path.isEmpty() || row < 0) {
        return QModelIndex();
    }
    const auto rootIdx = _file_system_model->index(_current_root_path);
    if (!rootIdx.isValid()) {
        return QModelIndex();
    }
    return _file_system_model->index(row, 0, rootIdx);
}

void ProjectViewModel::setProjectListMode(bool enabled) {
    if (_project_list_mode != enabled) {
        _project_list_mode = enabled;
        if (enabled) {
            // Clear current root path when switching to list mode
            _current_root_path.clear();
            if (_file_system_model) {
                _file_system_model->setRootPath(QString());
            }
            emit rootIndexChanged();
        } else {
            // Restore project root when switching back to tree mode
            updateRootPath();
        }
    }
}

void ProjectViewModel::openProjectFromList(const QString &projectPath) {
    if (!_project_service || projectPath.isEmpty()) {
        return;
    }

    ProjectOpenOptions options;
    options.loadUserPreferences = true;
    options.loadEditorSession = true;

    if (!_project_service->openProject(projectPath, options)) {
        qWarning() << "ProjectViewModel::openProjectFromList: Failed to open project:"
                   << _project_service->lastError();
        return;
    }

    // Switch back to tree mode
    setProjectListMode(false);
}

}// namespace rbc