#pragma once

#include <QObject>
#include <QStackedWidget>
#include <QMap>
#include <QWidget>

#include "RBCEditorRuntime/runtime/WorkflowManager.h"

class QMainWindow;

namespace rbc {

class EditorContext;
class WorkflowState;

/**
 * WorkflowContainerManager - 工作流容器管理器
 * 
 * 负责：
 * - 管理每个 Workflow 的中央容器（CentralWidget）
 * - 使用 QStackedWidget 在容器之间切换
 * - 避免 Widget 在不同 parent 之间转移的问题
 * 
 * 核心思想：
 * - 每个 Workflow 有自己固定的容器
 * - Widget（如 Viewport, NodeEditor）固定在各自的容器中
 * - 切换 Workflow 时只需切换 StackedWidget 的当前页面
 */
class RBC_EDITOR_RUNTIME_API WorkflowContainerManager : public QObject {
    Q_OBJECT

public:
    explicit WorkflowContainerManager(QMainWindow* mainWindow,
                                      EditorContext* context,
                                      QObject* parent = nullptr);
    ~WorkflowContainerManager() override = default;

    /**
     * 初始化：创建 StackedWidget 并设置为 CentralWidget
     */
    void initialize();

    /**
     * 切换到指定 Workflow 的容器
     */
    void switchContainer(WorkflowType type);

    /**
     * 注册一个 Workflow 的容器
     */
    void registerContainer(WorkflowType type, QWidget* container);

    /**
     * 获取 StackedWidget（用于调试或高级操作）
     */
    QStackedWidget* stackedWidget() const { return stackedWidget_; }

private:
    QMainWindow* mainWindow_;
    EditorContext* context_;
    QStackedWidget* stackedWidget_;
    QMap<WorkflowType, int> containerIndices_;// Workflow -> StackedWidget index
};

}// namespace rbc

