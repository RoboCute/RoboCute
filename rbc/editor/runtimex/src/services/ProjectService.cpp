#include "RBCEditorRuntime/services/ProjectService.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace rbc {

static QDateTime parse_iso_datetime_or_empty(const QJsonValue &v) {
    if (!v.isString()) {
        return {};
    }
    // Qt ISO parser handles `Z` suffix.
    const auto dt = QDateTime::fromString(v.toString(), Qt::ISODate);
    return dt.isValid() ? dt : QDateTime{};
}

static QString join_clean(const QString &a, const QString &b) {
    if (a.isEmpty()) {
        return QDir::cleanPath(b);
    }
    if (b.isEmpty()) {
        return QDir::cleanPath(a);
    }
    return QDir::cleanPath(QDir(a).filePath(b));
}

ProjectService::ProjectService(QObject *parent) : IProjectService(parent) {
}

bool ProjectService::isOpen() const { return open_; }

QString ProjectService::projectRoot() const { return projectRoot_; }

QString ProjectService::projectFilePath() const { return projectFilePath_; }

ProjectInfo ProjectService::projectInfo() const { return info_; }

QString ProjectService::lastError() const { return lastError_; }

bool ProjectService::openProject(const QString &projectRootOrProjectFile, const ProjectOpenOptions &options) {
    setLastError(QString());

    if (projectRootOrProjectFile.trimmed().isEmpty()) {
        setLastError("openProject: empty path");
        return false;
    }

    // If already open, close first (save session by default).
    if (open_) {
        closeProject(true);
    }

    QFileInfo fi(projectRootOrProjectFile);
    QString projectFile;
    QString rootDir;

    if (fi.exists() && fi.isFile()) {
        projectFile = fi.absoluteFilePath();
        rootDir = fi.absoluteDir().absolutePath();
    } else {
        // treat as directory
        QDir dir(projectRootOrProjectFile);
        if (!dir.exists()) {
            setLastError(QString("openProject: directory not found: %1").arg(projectRootOrProjectFile));
            return false;
        }
        rootDir = dir.absolutePath();
        projectFile = dir.filePath("rbc_project.json");
    }

    projectFile = QDir::cleanPath(projectFile);
    rootDir = QDir::cleanPath(rootDir);

    if (!QFileInfo::exists(projectFile)) {
        setLastError(QString("openProject: project file not found: %1").arg(projectFile));
        return false;
    }

    if (!loadProjectFile(projectFile)) {
        // lastError already set
        return false;
    }

    projectRoot_ = rootDir;
    projectFilePath_ = projectFile;
    open_ = true;

    // Ensure editor data dir exists (placeholder)
    QDir().mkpath(editorDataDirPath());

    if (options.loadUserPreferences) {
        loadUserPreferences();
    }
    if (options.loadEditorSession) {
        loadEditorSession();
    }

    emit projectOpened();
    emit projectInfoChanged();
    emit openedNodeGraphsChanged();
    emit activeNodeGraphChanged();
    return true;
}

void ProjectService::closeProject(bool saveSession) {
    if (!open_) {
        clearState();
        return;
    }

    emit projectClosing();

    if (saveSession) {
        // best-effort
        saveEditorSession();
        saveUserPreferences();
    }

    clearState();

    emit projectClosed();
    emit projectInfoChanged();
    emit openedNodeGraphsChanged();
    emit activeNodeGraphChanged();
}

bool ProjectService::reloadProject() {
    if (!open_ || projectFilePath_.isEmpty()) {
        setLastError("reloadProject: no project opened");
        return false;
    }
    return loadProjectFile(projectFilePath_);
}

QString ProjectService::resolvePath(const QString &projectRelativePath) const {
    if (projectRelativePath.isEmpty()) {
        return projectRelativePath;
    }
    QFileInfo fi(projectRelativePath);
    if (fi.isAbsolute()) {
        return QDir::cleanPath(fi.absoluteFilePath());
    }
    return join_clean(projectRoot_, projectRelativePath);
}

QString ProjectService::normalizeProjectPath(const QString &path) const {
    if (path.isEmpty()) {
        return path;
    }
    QFileInfo fi(path);
    if (!fi.isAbsolute()) {
        return QDir::cleanPath(path);
    }
    const auto abs = QDir::cleanPath(fi.absoluteFilePath());
    const auto root = QDir::cleanPath(projectRoot_);
    if (!root.isEmpty() && abs.startsWith(root, Qt::CaseInsensitive)) {
        QDir r(root);
        return QDir::cleanPath(r.relativeFilePath(abs));
    }
    return abs;
}

QJsonObject ProjectService::userPreferences() const { return userPrefs_; }

void ProjectService::setUserPreferences(const QJsonObject &prefs) {
    userPrefs_ = prefs;
    emit userPreferencesChanged();
}

bool ProjectService::loadUserPreferences() {
    setLastError(QString());
    if (!open_) {
        setLastError("loadUserPreferences: no project opened");
        return false;
    }

    QJsonObject obj;
    const auto file = userPrefsFilePath();
    if (!QFileInfo::exists(file)) {
        // no prefs yet: treat as success
        userPrefs_ = QJsonObject{};
        emit userPreferencesChanged();
        return true;
    }

    if (!readJsonFile(file, obj)) {
        return false;
    }

    userPrefs_ = obj;
    emit userPreferencesChanged();
    return true;
}

bool ProjectService::saveUserPreferences() {
    setLastError(QString());
    if (!open_) {
        setLastError("saveUserPreferences: no project opened");
        return false;
    }
    QDir().mkpath(editorDataDirPath());
    return writeJsonFile(userPrefsFilePath(), userPrefs_);
}

QStringList ProjectService::openedNodeGraphs() const { return openedGraphs_; }

QString ProjectService::activeNodeGraph() const { return activeGraph_; }

bool ProjectService::isNodeGraphOpen(const QString &graphPath) const {
    const auto p = QDir::cleanPath(normalizeProjectPath(graphPath));
    return openedGraphs_.contains(p);
}

bool ProjectService::isNodeGraphDirty(const QString &graphPath) const {
    const auto p = QDir::cleanPath(normalizeProjectPath(graphPath));
    return graphDirty_.value(p, false);
}

bool ProjectService::openNodeGraph(const QString &graphPath) {
    setLastError(QString());
    if (!open_) {
        setLastError("openNodeGraph: no project opened");
        return false;
    }

    const auto p = normalizeGraphPathOrSetError(graphPath);
    if (p.isEmpty()) {
        return false;
    }

    if (!openedGraphs_.contains(p)) {
        openedGraphs_.push_back(p);
        graphDirty_.insert(p, false);
        emit openedNodeGraphsChanged();
        emit nodeGraphOpened(p);
    }

    setActiveNodeGraph(p);
    return true;
}

bool ProjectService::closeNodeGraph(const QString &graphPath, bool allowDirty) {
    setLastError(QString());
    if (!open_) {
        setLastError("closeNodeGraph: no project opened");
        return false;
    }

    const auto p = QDir::cleanPath(normalizeProjectPath(graphPath));
    if (!openedGraphs_.contains(p)) {
        return true; // nothing to close
    }
    if (!allowDirty && graphDirty_.value(p, false)) {
        setLastError(QString("closeNodeGraph: graph is dirty: %1").arg(p));
        return false;
    }

    openedGraphs_.removeAll(p);
    graphDirty_.remove(p);

    if (activeGraph_ == p) {
        activeGraph_.clear();
        if (!openedGraphs_.isEmpty()) {
            activeGraph_ = openedGraphs_.last();
        }
        emit activeNodeGraphChanged();
    }

    emit openedNodeGraphsChanged();
    emit nodeGraphClosed(p);
    return true;
}

void ProjectService::closeAllNodeGraphs(bool allowDirty) {
    if (!open_) {
        return;
    }
    // Close in reverse order to preserve a reasonable "recent" behavior.
    const auto graphs = openedGraphs_;
    for (int i = graphs.size() - 1; i >= 0; --i) {
        closeNodeGraph(graphs[i], allowDirty);
    }
}

void ProjectService::setActiveNodeGraph(const QString &graphPath) {
    if (!open_) {
        return;
    }
    const auto p = QDir::cleanPath(normalizeProjectPath(graphPath));
    if (p.isEmpty()) {
        return;
    }
    if (!openedGraphs_.contains(p)) {
        // If asked to activate a non-open graph, open it.
        openNodeGraph(p);
        return;
    }
    if (activeGraph_ != p) {
        activeGraph_ = p;
        emit activeNodeGraphChanged();
    }
}

void ProjectService::markNodeGraphDirty(const QString &graphPath, bool dirty) {
    if (!open_) {
        return;
    }
    const auto p = QDir::cleanPath(normalizeProjectPath(graphPath));
    if (!openedGraphs_.contains(p)) {
        return;
    }
    if (graphDirty_.value(p, false) != dirty) {
        graphDirty_.insert(p, dirty);
        emit nodeGraphDirtyChanged(p, dirty);
    }
}

QStringList ProjectService::discoverNodeGraphs(const QString &subdir) const {
    if (!open_) {
        return {};
    }

    QString rel = subdir;
    if (rel.isEmpty()) {
        rel = join_clean(info_.paths.assets, "graphs");
    }
    const auto absDir = resolvePath(rel);
    if (!QDir(absDir).exists()) {
        return {};
    }

    QStringList results;
    QDirIterator it(absDir, QStringList() << "*.rbcgraph", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto abs = it.next();
        results.push_back(normalizeProjectPath(abs));
    }
    results.removeDuplicates();
    results.sort();
    return results;
}

bool ProjectService::loadEditorSession() {
    setLastError(QString());
    if (!open_) {
        setLastError("loadEditorSession: no project opened");
        return false;
    }

    const auto file = sessionFilePath();
    if (!QFileInfo::exists(file)) {
        return true; // no session yet
    }

    QJsonObject obj;
    if (!readJsonFile(file, obj)) {
        return false;
    }

    // Reset graph state before applying session
    openedGraphs_.clear();
    graphDirty_.clear();
    activeGraph_.clear();

    const auto graphsVal = obj.value("opened_graphs");
    if (graphsVal.isArray()) {
        for (const auto &v : graphsVal.toArray()) {
            if (!v.isString()) {
                continue;
            }
            const auto p = QDir::cleanPath(v.toString());
            if (p.isEmpty()) {
                continue;
            }
            if (!openedGraphs_.contains(p)) {
                openedGraphs_.push_back(p);
                graphDirty_.insert(p, false);
            }
        }
    }

    const auto activeVal = obj.value("active_graph");
    if (activeVal.isString()) {
        const auto p = QDir::cleanPath(activeVal.toString());
        if (!p.isEmpty() && openedGraphs_.contains(p)) {
            activeGraph_ = p;
        }
    }
    if (activeGraph_.isEmpty() && !openedGraphs_.isEmpty()) {
        activeGraph_ = openedGraphs_.last();
    }

    emit openedNodeGraphsChanged();
    emit activeNodeGraphChanged();
    return true;
}

bool ProjectService::saveEditorSession() {
    setLastError(QString());
    if (!open_) {
        setLastError("saveEditorSession: no project opened");
        return false;
    }

    QDir().mkpath(editorDataDirPath());

    QJsonObject obj;
    QJsonArray graphsArr;
    for (const auto &g : openedGraphs_) {
        graphsArr.push_back(g);
    }
    obj.insert("opened_graphs", graphsArr);
    obj.insert("active_graph", activeGraph_);
    obj.insert("saved_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    return writeJsonFile(sessionFilePath(), obj);
}

QString ProjectService::editorDataDirPath() const {
    // Place editor-only data under intermediate dir; can be changed later.
    const auto intermediate = info_.paths.intermediate.isEmpty() ? ".rbc" : info_.paths.intermediate;
    return join_clean(projectRoot_, join_clean(intermediate, "editor"));
}

QString ProjectService::userPrefsFilePath() const {
    return join_clean(editorDataDirPath(), "user_prefs.json");
}

QString ProjectService::sessionFilePath() const {
    return join_clean(editorDataDirPath(), "session.json");
}

bool ProjectService::readJsonFile(const QString &filePath, QJsonObject &outObj) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        setLastError(QString("readJsonFile: failed to open: %1").arg(filePath));
        return false;
    }
    const auto bytes = f.readAll();
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        setLastError(QString("readJsonFile: parse error in %1: %2").arg(filePath, err.errorString()));
        return false;
    }
    outObj = doc.object();
    return true;
}

bool ProjectService::writeJsonFile(const QString &filePath, const QJsonObject &obj) {
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(QString("writeJsonFile: failed to open: %1").arg(filePath));
        return false;
    }
    const QJsonDocument doc(obj);
    f.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

void ProjectService::setLastError(const QString &msg) {
    lastError_ = msg;
    if (!msg.isEmpty()) {
        qWarning() << "[ProjectService]" << msg;
    }
}

bool ProjectService::loadProjectFile(const QString &projectFilePath) {
    setLastError(QString());

    QJsonObject obj;
    if (!readJsonFile(projectFilePath, obj)) {
        return false;
    }

    // Fill ProjectInfo (best-effort; tolerate missing fields)
    ProjectInfo info;
    info.raw = obj;
    info.name = obj.value("name").toString();
    info.version = obj.value("version").toString();
    info.rbcVersion = obj.value("rbc_version").toString();
    info.author = obj.value("author").toString();
    info.description = obj.value("description").toString();
    info.createdAt = parse_iso_datetime_or_empty(obj.value("created_at"));
    info.modifiedAt = parse_iso_datetime_or_empty(obj.value("modified_at"));

    const auto pathsVal = obj.value("paths");
    if (pathsVal.isObject()) {
        const auto p = pathsVal.toObject();
        info.paths.assets = p.value("assets").toString(info.paths.assets);
        info.paths.docs = p.value("docs").toString(info.paths.docs);
        info.paths.datasets = p.value("datasets").toString(info.paths.datasets);
        info.paths.pretrained = p.value("pretrained").toString(info.paths.pretrained);
        info.paths.intermediate = p.value("intermediate").toString(info.paths.intermediate);
    }

    const auto cfgVal = obj.value("config");
    if (cfgVal.isObject()) {
        const auto c = cfgVal.toObject();
        info.config.defaultScene = c.value("default_scene").toString();
        info.config.startupGraph = c.value("startup_graph").toString();
        info.config.backend = c.value("backend").toString();
        info.config.resourceVersion = c.value("resource_version").toString();
        info.config.extra = c;
    }

    info_ = info;
    emit projectInfoChanged();
    return true;
}

void ProjectService::clearState() {
    open_ = false;
    projectRoot_.clear();
    projectFilePath_.clear();
    info_ = ProjectInfo{};
    lastError_.clear();
    userPrefs_ = QJsonObject{};
    openedGraphs_.clear();
    activeGraph_.clear();
    graphDirty_.clear();
}

QString ProjectService::normalizeGraphPathOrSetError(const QString &graphPath) {
    const auto rel = QDir::cleanPath(normalizeProjectPath(graphPath));
    if (rel.isEmpty()) {
        setLastError("openNodeGraph: empty graphPath");
        return {};
    }

    // If it's relative, validate extension and existence (best-effort).
    // We still allow opening non-existing paths in the future for "new graph" flows,
    // but for now keep it strict to avoid confusing UI states.
    if (!rel.endsWith(".rbcgraph", Qt::CaseInsensitive)) {
        setLastError(QString("openNodeGraph: not a .rbcgraph: %1").arg(rel));
        return {};
    }

    const auto abs = resolvePath(rel);
    if (!QFileInfo::exists(abs)) {
        setLastError(QString("openNodeGraph: file not found: %1").arg(abs));
        return {};
    }

    return rel;
}

}// namespace rbc