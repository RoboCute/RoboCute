#include "RBCEditorRuntime/runtime/Text2ImageWorkflowState.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/components/NodeEditor.h"
#include "RBCEditorRuntime/components/ViewportWidget.h"

#include <QVBoxLayout>
#include <QDebug>

namespace rbc {

Text2ImageWorkflowState::Text2ImageWorkflowState(QObject* parent)
    : WorkflowState(parent),
      centralContainer_(nullptr) {
}

Text2ImageWorkflowState::~Text2ImageWorkflowState() {
    // centralContainer_ 的生命周期由 Qt 的 parent 管理
}

void Text2ImageWorkflowState::enter(EditorContext* context) {
    qDebug() << "Entering Text2ImageWorkflowState";

    // 创建或初始化中央容器（这会确保 widget 的 parent 正确）
    createCentralContainer(context);

    // 确保 NodeEditor 可见
    if (context->nodeEditor) {
        context->nodeEditor->setVisible(true);
        context->nodeEditor->show();
    }

    // 确保 ViewportWidget 被隐藏（Text2Image 工作流不需要 Viewport）
    if (context->viewportWidget) {
        context->viewportWidget->setVisible(false);
        context->viewportWidget->hide();
    }

    // 建立信号连接（如果需要）
    // addConnection(...);
}

void Text2ImageWorkflowState::exit(EditorContext* context) {
    qDebug() << "Exiting Text2ImageWorkflowState";

    // 断开所有信号连接
    disconnectAll();

    // 注意：不隐藏 widget，因为它们会在其他 workflow 中被重新设置 parent
    // Widget 的可见性由新进入的 workflow 状态管理
}

QWidget* Text2ImageWorkflowState::getCentralContainer() {
    return centralContainer_;
}

QList<QDockWidget*> Text2ImageWorkflowState::getVisibleDocks(EditorContext* context) {
    QList<QDockWidget*> docks;

    // Text2Image 工作流下显示 Connection Status 和 Results
    if (context->connectionStatusDock) {
        docks.append(context->connectionStatusDock);
    }

    return docks;
}

QList<QDockWidget*> Text2ImageWorkflowState::getHiddenDocks(EditorContext* context) {
    QList<QDockWidget*> docks;

    // 隐藏与场景编辑相关的 Dock
    if (context->viewportDock) {
        docks.append(context->viewportDock);
    }

    // 注意：Scene Hierarchy, Details, Results 等可以根据需求决定是否隐藏
    // 这里暂时不隐藏，让用户可以看到结果

    return docks;
}

void Text2ImageWorkflowState::createCentralContainer(EditorContext* context) {
    // 创建中央容器（如果还未创建）
    if (!centralContainer_) {
        centralContainer_ = new QWidget();
        centralContainer_->setObjectName("Text2ImageContainer");

        auto* layout = new QVBoxLayout(centralContainer_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        centralContainer_->setLayout(layout);
    }

    // 确保 NodeEditor 正确添加到容器
    // 即使容器已存在，也需要确保 widget 的 parent 正确（可能从其他 workflow 切换回来）
    if (context->nodeEditor) {
        QLayout* layout = centralContainer_->layout();
        if (layout) {
            // 检查 NodeEditor 是否已经在 layout 中
            bool foundInLayout = false;
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem* item = layout->itemAt(i);
                if (item && item->widget() == context->nodeEditor) {
                    foundInLayout = true;
                    break;
                }
            }

            // 如果 NodeEditor 不在 layout 中，需要添加
            if (!foundInLayout) {
                // 如果 widget 已经有其他 parent，Qt 的 setParent() 会自动从旧 parent 移除
                context->nodeEditor->setParent(centralContainer_);
                context->nodeEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                layout->addWidget(context->nodeEditor);
            } else if (context->nodeEditor->parent() != centralContainer_) {
                // 如果 widget 在 layout 中但 parent 不对，修正 parent
                context->nodeEditor->setParent(centralContainer_);
            }
        }
    }

    // 确保 ViewportWidget 被隐藏（Text2Image 工作流不需要 Viewport）
    if (context->viewportWidget) {
        context->viewportWidget->setVisible(false);
        context->viewportWidget->hide();
    }
}

}// namespace rbc

