#pragma once

#include <QObject>
#include <QWidget>
#include <QDockWidget>
#include <QList>
#include <QMetaObject>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"

namespace rbc {

class EditorContext;

/**
 * WorkflowState - 抽象基类，表示一个 Workflow 状态
 * 
 * 每个具体的 Workflow（如 SceneEditing、Text2Image）都是一个状态
 * 状态负责管理该 Workflow 下的：
 * - UI 布局配置
 * - Widget 的可见性
 * - 信号连接
 * - 资源的生命周期
 */
class RBC_EDITOR_RUNTIME_API WorkflowState : public QObject {
    Q_OBJECT

public:
    explicit WorkflowState(QObject* parent = nullptr);
    virtual ~WorkflowState();

    /**
     * 进入该状态时调用
     * 用于：建立信号连接、初始化 UI、显示相关组件
     */
    virtual void enter(EditorContext* context) = 0;

    /**
     * 退出该状态时调用
     * 用于：断开信号连接、清理资源、隐藏组件
     */
    virtual void exit(EditorContext* context) = 0;

    /**
     * 获取该状态的中央容器
     * 该容器将被设置为 MainWindow 的 CentralWidget
     */
    virtual QWidget* getCentralContainer() = 0;

    /**
     * 获取该状态下应该可见的 Dock
     */
    virtual QList<QDockWidget*> getVisibleDocks(EditorContext* context) = 0;

    /**
     * 获取该状态下应该隐藏的 Dock
     */
    virtual QList<QDockWidget*> getHiddenDocks(EditorContext* context) = 0;

    /**
     * 获取状态类型
     */
    virtual WorkflowType getType() const = 0;

    /**
     * 获取状态名称
     */
    virtual QString getName() const = 0;

protected:
    /**
     * 添加需要管理的信号连接
     * 这些连接会在 exit() 时自动断开
     */
    void addConnection(const QMetaObject::Connection& connection);

    /**
     * 清理所有信号连接
     */
    void disconnectAll();

private:
    // 管理的信号连接
    QList<QMetaObject::Connection> connections_;
};

}// namespace rbc

