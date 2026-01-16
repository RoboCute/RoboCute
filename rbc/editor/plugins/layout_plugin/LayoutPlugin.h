#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/services/LayoutService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

namespace rbc {

/**
 * LayoutViewModel - Layout 管理视图模型
 * 
 * 负责管理 Layout 切换和视图可见性控制
 */
class RBC_EDITOR_PLUGIN_API LayoutViewModel : public ViewModelBase {
    Q_OBJECT
    Q_PROPERTY(QString currentLayoutId READ currentLayoutId NOTIFY currentLayoutIdChanged)
    Q_PROPERTY(QStringList availableLayouts READ availableLayouts NOTIFY availableLayoutsChanged)
    Q_PROPERTY(QVariantList viewStates READ viewStates NOTIFY viewStatesChanged)

public:
    explicit LayoutViewModel(LayoutService *layoutService, QObject *parent = nullptr);
    ~LayoutViewModel() override;

    // Property accessors
    QString currentLayoutId() const;
    QStringList availableLayouts() const;
    QVariantList viewStates() const;

    // QML invokable methods
    Q_INVOKABLE void switchLayout(const QString &layoutId);
    Q_INVOKABLE void setViewVisible(const QString &viewId, bool visible);
    Q_INVOKABLE bool isViewVisible(const QString &viewId) const;
    Q_INVOKABLE QString getLayoutName(const QString &layoutId) const;
    Q_INVOKABLE QString getLayoutDescription(const QString &layoutId) const;

    // Service access
    LayoutService *layoutService() const { return layoutService_; }

signals:
    void currentLayoutIdChanged();
    void availableLayoutsChanged();
    void viewStatesChanged();

private slots:
    void onLayoutChanged();

private:
    void updateViewStates();
    
    LayoutService *layoutService_ = nullptr;
    QVariantList viewStates_;
};

/**
 * LayoutPlugin - Layout 管理插件
 * 
 * 提供 Layout 切换和视图可见性控制的 UI 面板和菜单
 */
class RBC_EDITOR_PLUGIN_API LayoutPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit LayoutPlugin(QObject *parent = nullptr);
    ~LayoutPlugin() override;

    // === Static Methods for Factory ===
    static QString staticPluginId() { return "com.robocute.layout"; }
    static QString staticPluginName() { return "Layout Plugin"; }

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

private:
    void buildMenuContributions();
    
    LayoutService *layoutService_ = nullptr;
    LayoutViewModel *viewModel_ = nullptr;
    PluginContext *context_ = nullptr;
    QList<MenuContribution> menuContributions_;
};

// 导出工厂函数
class IPluginFactory;
LUISA_EXPORT_API IPluginFactory *createPluginFactory();

}// namespace rbc
