#pragma once

#include <rbc_config.h>
#include "RBCEditorRuntime/services/IProjectService.h"

#include <QHash>
#include <QJsonObject>

namespace rbc {

class RBC_EDITOR_RUNTIME_API ProjectService : public IProjectService {
    Q_OBJECT

    // Minimal QML-friendly properties for editor UI.
    Q_PROPERTY(bool open READ isOpen NOTIFY projectInfoChanged)
    Q_PROPERTY(QString projectRoot READ projectRoot NOTIFY projectInfoChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectInfoChanged)
    Q_PROPERTY(QString activeNodeGraph READ activeNodeGraph NOTIFY activeNodeGraphChanged)
    Q_PROPERTY(QStringList openedNodeGraphs READ openedNodeGraphs NOTIFY openedNodeGraphsChanged)

public:
    explicit ProjectService(QObject *parent = nullptr);
    ~ProjectService() override = default;

    // -- IProjectService --
    bool isOpen() const override;
    QString projectRoot() const override;
    QString projectFilePath() const override;
    ProjectInfo projectInfo() const override;
    QString lastError() const override;

    bool openProject(const QString &projectRootOrProjectFile,
                     const ProjectOpenOptions &options = ProjectOpenOptions()) override;
    void closeProject(bool saveSession = true) override;
    bool reloadProject() override;

    QString resolvePath(const QString &projectRelativePath) const override;
    QString normalizeProjectPath(const QString &path) const override;

    QJsonObject userPreferences() const override;
    void setUserPreferences(const QJsonObject &prefs) override;
    bool loadUserPreferences() override;
    bool saveUserPreferences() override;

    QStringList openedNodeGraphs() const override;
    QString activeNodeGraph() const override;
    bool isNodeGraphOpen(const QString &graphPath) const override;
    bool isNodeGraphDirty(const QString &graphPath) const override;

    bool openNodeGraph(const QString &graphPath) override;
    bool closeNodeGraph(const QString &graphPath, bool allowDirty = false) override;
    void closeAllNodeGraphs(bool allowDirty = false) override;
    void setActiveNodeGraph(const QString &graphPath) override;
    void markNodeGraphDirty(const QString &graphPath, bool dirty) override;

    QStringList discoverNodeGraphs(const QString &subdir = QString()) const override;

    bool loadEditorSession() override;
    bool saveEditorSession() override;

public:
    // Convenience getters
    QString projectName() const { return info_.name; }

private:
    // Files/dirs (placeholder layout, can be refactored later)
    QString editorDataDirPath() const;     // {project}/.rbc/editor
    QString userPrefsFilePath() const;     // .../user_prefs.json
    QString sessionFilePath() const;       // .../session.json

    bool readJsonFile(const QString &filePath, QJsonObject &outObj);
    bool writeJsonFile(const QString &filePath, const QJsonObject &obj);
    void setLastError(const QString &msg);

    bool loadProjectFile(const QString &projectFilePath);
    void clearState();

    // Normalize a NodeGraph path to project-relative, cleaned form.
    QString normalizeGraphPathOrSetError(const QString &graphPath);

private:
    bool open_ = false;
    QString projectRoot_;
    QString projectFilePath_;
    ProjectInfo info_;
    QString lastError_;

    QJsonObject userPrefs_;

    QStringList openedGraphs_;         // project-relative paths
    QString activeGraph_;              // project-relative path
    QHash<QString, bool> graphDirty_;  // graphPath -> dirty
};

}// namespace rbc