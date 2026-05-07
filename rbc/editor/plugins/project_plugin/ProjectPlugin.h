#pragma once

#include <rbc_config.h>
#include "RBCEditorRuntime/services/IProjectService.h"
#include "RBCEditorRuntime/services/ProjectService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include <QFileSystemModel>
#include <QWidget>
#include <QJsonObject>
#include <QStringList>
#include <QStandardPaths>
#include <QPointer>

namespace rbc {

class RBC_EDITOR_PLUGIN_API ProjectViewModel : public ViewModelBase {
    Q_OBJECT
    Q_PROPERTY(QString projectRoot READ projectRoot NOTIFY projectRootChanged)
    Q_PROPERTY(QFileSystemModel *fileSystemModel READ fileSystemModel CONSTANT)
    Q_PROPERTY(QModelIndex rootIndex READ rootIndex NOTIFY rootIndexChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    explicit ProjectViewModel(IProjectService *projectService, QObject *parent = nullptr);
    ~ProjectViewModel() override;

    // Property accessors
    QString projectRoot() const;
    QFileSystemModel *fileSystemModel() const { return _file_system_model; }
    QModelIndex rootIndex() const;
    QString filter() const { return _filter; }
    void setFilter(const QString &filter);

    // QML invokable methods
    Q_INVOKABLE QString getFilePath(const QModelIndex &index) const;
    Q_INVOKABLE bool isDirectory(const QModelIndex &index) const;
    Q_INVOKABLE QString getFileName(const QModelIndex &index) const;
    Q_INVOKABLE void setRootPath(const QString &path);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void navigateUp();
    Q_INVOKABLE bool canNavigateUp() const;

    // Helper methods for QML (using row index)
    Q_INVOKABLE QString getFilePathByRow(int row) const;
    Q_INVOKABLE bool isDirectoryByRow(int row) const;
    Q_INVOKABLE QString getFileNameByRow(int row) const;
    Q_INVOKABLE int rowCount() const;
    Q_INVOKABLE QModelIndex indexForRow(int row) const;

    // Service access
    IProjectService *projectService() const { return _project_service; }

    // Project list mode support
    Q_INVOKABLE void setProjectListMode(bool enabled);
    Q_INVOKABLE bool isProjectListMode() const { return _project_list_mode; }
    Q_INVOKABLE QStringList recentProjects() const { return _recent_projects; }
    Q_INVOKABLE void setRecentProjects(const QStringList &projects) { _recent_projects = projects; }
    Q_INVOKABLE void openProjectFromList(const QString &projectPath);

signals:
    void projectRootChanged();
    void rootIndexChanged();
    void filterChanged();

private slots:
    void onProjectOpened();
    void onProjectClosed();
    void onProjectInfoChanged();

private:
    void updateRootPath();
    void setupFilters();

    IProjectService *_project_service = nullptr;
    QFileSystemModel *_file_system_model = nullptr;
    QString _filter = "*";// Default: show all files
    QString _current_root_path;
    bool _project_list_mode = false;
    QStringList _recent_projects;
};

class RBC_EDITOR_PLUGIN_API ProjectPlugin : public IEditorPlugin {
    Q_OBJECT
    Q_PROPERTY(QWidget *fileBrowserWidget READ fileBrowserWidget CONSTANT)

public:
    explicit ProjectPlugin(QObject *parent = nullptr);
    ~ProjectPlugin() override;

    // === Static Methods for Factory ===
    static QString staticPluginId() { return "com.robocute.project_plugin"; }
    static QString staticPluginName() { return "Project Plugin"; }

    // IEditorPlugin interface
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }

    QList<ViewContribution> view_contributions() const override;
    QList<NativeViewContribution> native_view_contributions() const override;
    QList<MenuContribution> menu_contributions() const override;
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }

    void register_view_models(QQmlEngine *engine) override;

    // Get ViewModel for a specific view
    QObject *getViewModel(const QString &viewId) override;

    // Get native widget for a specific view
    QWidget *getNativeWidget(const QString &viewId) override;

    // Get file browser widget (for native widget dock) - deprecated, use getNativeWidget("project_file_browser") instead
    QWidget *fileBrowserWidget() const { return _file_browser_widget.data(); }

private slots:
    void onOpenProjectTriggered();

private:
    // Project cache management
    QString getWorkDir() const;
    QString getCacheFilePath() const;
    void loadProjectCache();
    void saveProjectCache();
    void addProjectToCache(const QString &projectPath);
    QStringList getRecentProjects() const;
    QString getLastOpenedProject() const;

    IProjectService *_project_service = nullptr;
    ProjectViewModel *_view_model = nullptr;
    PluginContext *_context = nullptr;
    QPointer<QWidget> _file_browser_widget;// Tracked with QPointer, auto-detects deletion

    // Pre-registered Native View Contributions
    QList<NativeViewContribution> _registered_contributions;

    // Store connection handles for explicit disconnection on unload
    // This is necessary because lambda connections without a context object
    // cannot be disconnected via disconnect(sender, nullptr, this, nullptr)
    QMetaObject::Connection _project_opened_connection;
    QMetaObject::Connection _tree_view_double_click_connection;
    QMetaObject::Connection _project_closing_connection;
};

// Export factory function (new design)
class IPluginFactory;
LUISA_EXPORT_API IPluginFactory *createPluginFactory();

}// namespace rbc
