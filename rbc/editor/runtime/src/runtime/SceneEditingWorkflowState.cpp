#include "RBCEditorRuntime/runtime/SceneEditingWorkflowState.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/components/ViewportWidget.h"
#include "RBCEditorRuntime/components/NodeEditor.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QDebug>
#include <QTimer>

namespace rbc {

SceneEditingWorkflowState::SceneEditingWorkflowState(QObject* parent)
    : WorkflowState(parent),
      centralContainer_(nullptr),
      splitter_(nullptr) {
}

SceneEditingWorkflowState::~SceneEditingWorkflowState() {
    // centralContainer_ 的生命周期由 Qt 的 parent 管理
    // 当它作为 CentralWidget 被移除时，或者 MainWindow 销毁时会自动清理
}

void SceneEditingWorkflowState::enter(EditorContext* context) {
    qDebug() << "Entering SceneEditingWorkflowState";

    // 创建或初始化中央容器
    createCentralContainer(context);

    // 确保 Viewport 和 NodeEditor 可见
    if (context->viewportWidget) {
        context->viewportWidget->setVisible(true);
        context->viewportWidget->show();
    }

    if (context->nodeEditor) {
        context->nodeEditor->setVisible(true);
        context->nodeEditor->show();
    }

    // 延迟设置 splitter 的初始大小，确保容器已经被正确显示和布局
    // 使用 QTimer::singleShot 延迟到事件循环的下一次迭代
    QTimer::singleShot(0, [this]() {
        setupSplitterSizes();
    });

    // 建立信号连接（如果需要）
    // 例如：连接 viewport 的选择事件等
    // addConnection(...);
}

void SceneEditingWorkflowState::exit(EditorContext* context) {
    qDebug() << "Exiting SceneEditingWorkflowState";

    // 断开所有信号连接
    disconnectAll();

    // 注意：不隐藏 widget，因为它们会在其他 workflow 中被重新设置 parent
    // 只是断开连接，避免在其他 workflow 中触发不必要的事件
    // Widget 的可见性由新进入的 workflow 状态管理
}

QWidget* SceneEditingWorkflowState::getCentralContainer() {
    return centralContainer_;
}

QList<QDockWidget*> SceneEditingWorkflowState::getVisibleDocks(EditorContext* context) {
    QList<QDockWidget*> docks;

    // Scene Editing 工作流下所有 Dock 都可见
    if (context->viewportDock) {
        docks.append(context->viewportDock);
    }
    // 注意：在新架构中，viewport 和 nodeEditor 在 central container 中，不作为 dock

    return docks;
}

QList<QDockWidget*> SceneEditingWorkflowState::getHiddenDocks(EditorContext* context) {
    QList<QDockWidget*> docks;

    // Scene Editing 工作流下没有需要隐藏的 Dock
    // 所有辅助面板（Scene Hierarchy, Details, Results 等）都保持可见

    return docks;
}

void SceneEditingWorkflowState::createCentralContainer(EditorContext* context) {
    // 创建中央容器（如果还未创建）
    if (!centralContainer_) {
        centralContainer_ = new QWidget();
        centralContainer_->setObjectName("SceneEditingContainer");

        auto* layout = new QVBoxLayout(centralContainer_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // 创建 Splitter，上面是 Viewport，下面是 NodeEditor
        splitter_ = new QSplitter(Qt::Vertical, centralContainer_);
        splitter_->setObjectName("SceneEditingSplitter");
        
        // 设置 splitter 属性，确保子 widget 可以正确调整大小
        splitter_->setChildrenCollapsible(false);// 防止子 widget 被折叠

        layout->addWidget(splitter_);
        centralContainer_->setLayout(layout);
    }

    // 确保 Viewport 和 NodeEditor 正确添加到 Splitter
    // 即使容器已存在，也需要确保 widget 的 parent 正确（可能从其他 workflow 切换回来）
    if (context->viewportWidget) {
        // 如果 ViewportWidget 不在 splitter_ 中，需要添加
        if (context->viewportWidget->parent() != splitter_) {
            // 如果 widget 已经有其他 parent，Qt 会自动从旧 parent 移除
            context->viewportWidget->setParent(splitter_);
            context->viewportWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            splitter_->addWidget(context->viewportWidget);
        }
    }

    if (context->nodeEditor) {
        // 如果 NodeEditor 不在 splitter_ 中，需要添加
        if (context->nodeEditor->parent() != splitter_) {
            // 如果 widget 已经有其他 parent，Qt 会自动从旧 parent 移除
            context->nodeEditor->setParent(splitter_);
            context->nodeEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            splitter_->addWidget(context->nodeEditor);
        }
    }
}

void SceneEditingWorkflowState::setupSplitterSizes() {
    if (!splitter_) {
        return;
    }

    // 设置 Viewport 占 70%，NodeEditor 占 30%
    QList<int> sizes;
    int totalHeight = splitter_->height();

    if (totalHeight > 0) {
        sizes << static_cast<int>(totalHeight * 0.7);
        sizes << static_cast<int>(totalHeight * 0.3);
        splitter_->setSizes(sizes);
    } else {
        // 如果还没有 height，使用默认值
        sizes << 700 << 300;
        splitter_->setSizes(sizes);
    }

    // 设置最小尺寸
    splitter_->setStretchFactor(0, 7);// Viewport 更有弹性
    splitter_->setStretchFactor(1, 3);// NodeEditor 较小弹性
}

}// namespace rbc

