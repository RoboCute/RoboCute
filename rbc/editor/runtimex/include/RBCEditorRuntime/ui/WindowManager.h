#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QPointer>
#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QHash>
#include "RBCEditorRuntime/plugins/PluginContributions.h"

namespace rbc {

class EditorPluginManager;

/**
 * @brief WindowManager - Qt Widget 生命周期管理范式
 * 
 * 设计原则：
 * 1. 遵循 Qt parent-child 所有权模型：子 widget 由父 widget 自动管理
 * 2. WindowManager 拥有 main_window_，main_window_ 拥有其所有子 widget
 * 3. 外部传入的 widget（如 plugin 创建的）使用 QPointer 追踪，不接管所有权
 * 4. cleanup() 必须在 plugin unload 之前调用，确保正确的析构顺序
 * 5. 析构器保持简单，依赖 Qt 自动清理机制
 */
class RBC_EDITOR_RUNTIME_API WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(EditorPluginManager *plugin_mng, QObject *parent = nullptr);
    ~WindowManager() override;

    // == Lifecycle Management ==
    /**
     * @brief 在 plugin unload 之前调用此方法进行清理
     * 
     * 这是关键的生命周期方法：
     * - 隐藏窗口停止渲染
     * - 清理 QML context 引用（打破对 ViewModel 的引用）
     * - 释放外部 widget 引用（让 plugin 管理其 widget）
     * - 不删除任何 widget，让析构器通过 Qt 机制处理
     */
    void cleanup();

    // == MainWindow Management ==
    QMainWindow *main_window() const { return main_window_; }
    void setup_main_window();

    // == Create Dockable Window View through Contribution (QML)
    QDockWidget *createDockableView(const ViewContribution &contribution, QObject *viewModel);

    // == Create Dockable Window View through native QWidget (C++ widget)
    // 注意：widget 的所有权转移给 DockWidget（即 main_window_）
    // 如果 widget 来自外部（如 plugin），调用者应确保 widget 在 WindowManager 之前不被销毁
    // 使用 takeExternalWidget=true 表示 widget 来自外部，WindowManager 不会在析构时删除它
    QDockWidget *createDockableView(
        const QString &viewId,
        const QString &title,
        QWidget *widget,
        Qt::DockWidgetArea dockArea,
        QDockWidget::DockWidgetFeatures features = (QDockWidget::DockWidgetClosable |
                                                   QDockWidget::DockWidgetMovable |
                                                   QDockWidget::DockWidgetFloatable),
        Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
        bool isExternalWidget = false);

    // == Create Standalone Window (for Preview)
    QWidget *createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title);

    // == Apply Menu Contributions from plugins
    void applyMenuContributions(const QList<MenuContribution> &contributions);

private:
    QDockWidget *createDockWidgetCommon(
        const QString &viewId,
        const QString &title,
        QWidget *content,
        Qt::DockWidgetArea dockArea,
        QDockWidget::DockWidgetFeatures features,
        Qt::DockWidgetAreas allowedAreas);

    void cleanupQmlWidget(QWidget *widget);

    QMainWindow *main_window_ = nullptr;
    EditorPluginManager *plugin_mng_ = nullptr;
    bool cleaned_up_ = false;
    
    // 追踪外部传入的 widget（使用 QPointer 安全追踪）
    // Key: viewId, Value: 外部 widget 的弱引用
    QHash<QString, QPointer<QWidget>> external_widgets_;
};

}// namespace rbc