#pragma once

#include <rbc_config.h>
#include "RBCEditorRuntime/services/IProjectService.h"
#include "RBCEditorRuntime/services/ProjectService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include <QFileSystemModel>
#include <QWidget>

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
    QFileSystemModel *fileSystemModel() const { return fileSystemModel_; }
    QModelIndex rootIndex() const;
    QString filter() const { return filter_; }
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
    IProjectService *projectService() const { return projectService_; }

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

    IProjectService *projectService_ = nullptr;
    QFileSystemModel *fileSystemModel_ = nullptr;
    QString filter_ = "*";// Default: show all files
    QString currentRootPath_;
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
    QList<MenuContribution> menu_contributions() const override;
    QList<ToolbarContribution> toolbar_contributions() const override { return {}; }

    void register_view_models(QQmlEngine *engine) override;

    // Get ViewModel for a specific view
    QObject *getViewModel(const QString &viewId) override;

    // Get file browser widget (for native widget dock)
    QWidget *fileBrowserWidget() const { return fileBrowserWidget_; }

private slots:
    void onOpenProjectTriggered();

private:
    IProjectService *projectService_ = nullptr;
    ProjectViewModel *viewModel_ = nullptr;
    PluginContext *context_ = nullptr;
    QWidget *fileBrowserWidget_ = nullptr;
    
    // 保存连接句柄，以便在 unload 时显式断开
    // 这是必要的，因为 lambda 连接如果没有 context 对象，
    // 调用 disconnect(sender, nullptr, this, nullptr) 无法断开它们
    QMetaObject::Connection projectOpenedConnection_;
    QMetaObject::Connection treeViewDoubleClickConnection_;
};

// 导出工厂函数（新设计）
class IPluginFactory;
LUISA_EXPORT_API IPluginFactory *createPluginFactory();

}// namespace rbc
