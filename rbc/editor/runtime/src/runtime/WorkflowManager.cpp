#include "RBCEditorRuntime/runtime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/WorkflowState.h"
#include "RBCEditorRuntime/runtime/SceneEditingWorkflowState.h"
#include "RBCEditorRuntime/runtime/Text2ImageWorkflowState.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"

#include <QDebug>

namespace rbc {

WorkflowManager::WorkflowManager(QObject* parent)
    : QObject(parent),
      context_(nullptr),
      currentState_(nullptr),
      isTransitioning_(false) {
}

WorkflowManager::~WorkflowManager() {
    // 清理所有状态
    qDeleteAll(states_);
    states_.clear();
}

void WorkflowManager::initialize(EditorContext* context) {
    Q_ASSERT(context != nullptr);
    context_ = context;

    // 创建所有状态
    createStates();

    // 注意：不在这里设置 currentState_，让它保持为 nullptr
    // 这样第一次调用 switchWorkflow() 时会正常执行 enter() 逻辑
    // 初始状态应该在 MainWindow::setupUi() 中通过 switchWorkflow() 设置
}

WorkflowType WorkflowManager::currentWorkflow() const {
    if (currentState_) {
        return currentState_->getType();
    }
    return WorkflowType::SceneEditing;// 默认值
}

bool WorkflowManager::switchWorkflow(WorkflowType workflow) {
    // 防止重入
    if (isTransitioning_) {
        qWarning() << "WorkflowManager: Already transitioning, ignoring switch request";
        return false;
    }

    // 检查是否已经是目标状态（但允许从 nullptr 状态切换到初始状态）
    if (currentState_ && currentState_->getType() == workflow) {
        qDebug() << "WorkflowManager: Already in workflow" << static_cast<int>(workflow);
        return false;
    }

    // 检查目标状态是否存在
    if (!states_.contains(workflow)) {
        qWarning() << "WorkflowManager: Unknown workflow type" << static_cast<int>(workflow);
        return false;
    }

    WorkflowState* newState = states_[workflow];
    WorkflowType oldWorkflow = currentState_ ? currentState_->getType() : WorkflowType::SceneEditing;

    qDebug() << "WorkflowManager: Switching from" << static_cast<int>(oldWorkflow) 
             << "to" << static_cast<int>(workflow);

    // 标记正在切换
    isTransitioning_ = true;

    try {
        // 1. 发送即将切换的信号
        emit workflowWillChange(workflow, oldWorkflow);

        // 2. 退出当前状态
        if (currentState_) {
            qDebug() << "WorkflowManager: Exiting state" << currentState_->getName();
            currentState_->exit(context_);
        }

        // 3. 切换状态
        currentState_ = newState;

        // 4. 进入新状态
        qDebug() << "WorkflowManager: Entering state" << currentState_->getName();
        currentState_->enter(context_);

        // 5. 发送已切换的信号
        emit workflowChanged(workflow, oldWorkflow);

        qDebug() << "WorkflowManager: Switch completed successfully";

        // 切换完成
        isTransitioning_ = false;
        return true;

    } catch (const std::exception& e) {
        qCritical() << "WorkflowManager: Exception during workflow switch:" << e.what();
        isTransitioning_ = false;
        return false;
    } catch (...) {
        qCritical() << "WorkflowManager: Unknown exception during workflow switch";
        isTransitioning_ = false;
        return false;
    }
}

WorkflowState* WorkflowManager::getState(WorkflowType type) const {
    return states_.value(type, nullptr);
}

void WorkflowManager::createStates() {
    // 创建 SceneEditing 状态
    auto* sceneEditingState = new SceneEditingWorkflowState(this);
    states_[WorkflowType::SceneEditing] = sceneEditingState;

    // 创建 Text2Image 状态
    auto* text2ImageState = new Text2ImageWorkflowState(this);
    states_[WorkflowType::Text2Image] = text2ImageState;

    qDebug() << "WorkflowManager: Created" << states_.size() << "workflow states";
}

}// namespace rbc
