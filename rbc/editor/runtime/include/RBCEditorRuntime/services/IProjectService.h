#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include "RBCEditorRuntime/infra/editor/EditorProject.h"
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

// === Data Models =============================================================

// Note: These are intentionally lightweight "transport" structs for editor/runtime layers.
// Future iterations can replace them with richer domain objects.

struct ProjectPaths {
    QString assets = "assets";
    QString docs = "docs";
    QString datasets = "datasets";
    QString pretrained = "pretrained";
    QString intermediate = ".rbc";
};

struct ProjectConfig {
    QString defaultScene;
    QString startupGraph;
    QString backend;
    QString resourceVersion;
    QJsonObject extra;
};

struct ProjectInfo {
    QString name;
    QString version;
    QString rbcVersion;
    QString author;
    QString description;
    QDateTime createdAt;
    QDateTime modifiedAt;
    ProjectPaths paths;
    ProjectConfig config;
    QJsonObject raw;// original `rbc_project.json` object
};

struct ProjectOpenOptions {
    bool loadUserPreferences = true;// project-level editor preference
    bool loadEditorSession = true;  // opened graphs, active graph, etc.
};

// === Service Interface =======================================================

/**
 * IProjectService - Project management service interface
 * 
 * 重要：作为接口类，使用内联析构器避免链接冲突
 * 具体实现类（如 ProjectService）应在 .cpp 中定义析构器
 */
class RBC_EDITOR_RUNTIME_API IProjectService : public IService {
    Q_OBJECT
public:
    explicit IProjectService(QObject *parent = nullptr) : IService(parent) {}

    // IService interface
    QString serviceId() const override { return "com.robocute.project_service"; }

    // -- State --
    virtual bool isOpen() const = 0;
    virtual QString projectRoot() const = 0;    // absolute directory path
    virtual QString projectFilePath() const = 0;// absolute file path (rbc_project.json)
    virtual ProjectInfo projectInfo() const = 0;
    virtual QString lastError() const = 0;

    // -- Open/Close --
    virtual bool openProject(const QString &projectRootOrProjectFile,
                             const ProjectOpenOptions &options = ProjectOpenOptions()) = 0;
    virtual void closeProject(bool saveSession = true) = 0;
    virtual bool reloadProject() = 0;

    // -- Path Helpers --
    // Convert `assets/xxx` to absolute path within current project root.
    virtual QString resolvePath(const QString &projectRelativePath) const = 0;
    // If absolute path is under project root, returns project-relative path, otherwise returns original.
    virtual QString normalizeProjectPath(const QString &path) const = 0;

    // -- Editor User Preferences (project-scoped) --
    // Stored under `{project}/.rbc/editor/user_prefs.json` (placeholder path, can change later).
    virtual QJsonObject userPreferences() const = 0;
    virtual void setUserPreferences(const QJsonObject &prefs) = 0;
    virtual bool loadUserPreferences() = 0;
    virtual bool saveUserPreferences() = 0;

    // -- NodeGraph Sessions (project-scoped) --
    // Here "graphPath" is expected to be a project-relative path like `assets/graphs/foo.rbcgraph`.
    virtual QStringList openedNodeGraphs() const = 0;
    virtual QString activeNodeGraph() const = 0;
    virtual bool isNodeGraphOpen(const QString &graphPath) const = 0;
    virtual bool isNodeGraphDirty(const QString &graphPath) const = 0;

    virtual bool openNodeGraph(const QString &graphPath) = 0;
    virtual bool closeNodeGraph(const QString &graphPath, bool allowDirty = false) = 0;
    virtual void closeAllNodeGraphs(bool allowDirty = false) = 0;
    virtual void setActiveNodeGraph(const QString &graphPath) = 0;
    virtual void markNodeGraphDirty(const QString &graphPath, bool dirty) = 0;

    // Discover existing `.rbcgraph` files in project (placeholder utility).
    virtual QStringList discoverNodeGraphs(const QString &subdir = QString()) const = 0;

    // -- Editor Session --
    // Stored under `{project}/.rbc/editor/session.json` (placeholder path, can change later).
    virtual bool loadEditorSession() = 0;
    virtual bool saveEditorSession() = 0;

signals:
    void projectOpened();
    void projectClosing();
    void projectClosed();
    void projectInfoChanged();

    void userPreferencesChanged();

    void openedNodeGraphsChanged();
    void activeNodeGraphChanged();
    void nodeGraphOpened(const QString &graphPath);
    void nodeGraphClosed(const QString &graphPath);
    void nodeGraphDirtyChanged(const QString &graphPath, bool dirty);
};

}// namespace rbc