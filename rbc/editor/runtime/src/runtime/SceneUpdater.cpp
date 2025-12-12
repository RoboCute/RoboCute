#include "RBCEditorRuntime/runtime/SceneUpdater.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/components/SceneHierarchyWidget.h"
#include "RBCEditorRuntime/components/ResultPanel.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/runtime/EventBus.h"
#include <QDebug>

namespace rbc {

SceneUpdater::SceneUpdater(EditorContext *context, QObject *parent)
    : QObject(parent),
      context_(context),
      sceneUpdatedSubscriptionId_(-1) {
    Q_ASSERT(context_ != nullptr);
}

SceneUpdater::~SceneUpdater() {
    // 取消订阅事件总线
    if (sceneUpdatedSubscriptionId_ >= 0) {
        EventBus::instance().unsubscribe(sceneUpdatedSubscriptionId_);
    }
}

void SceneUpdater::initialize() {
    // 订阅场景更新事件
    sceneUpdatedSubscriptionId_ = EventBus::instance().subscribe(
        EventType::SceneUpdated,
        [this](const Event &event) {
            onEvent(event);
        });
}

void SceneUpdater::handleSceneUpdated() {
    if (!context_->sceneSyncManager) {
        qWarning() << "SceneUpdater: SceneSyncManager is not available";
        return;
    }

    const auto *sceneSync = context_->sceneSyncManager->sceneSync();
    if (!sceneSync) {
        qWarning() << "SceneUpdater: SceneSync is not available";
        return;
    }

    // Update scene hierarchy
    if (context_->sceneHierarchy) {
        context_->sceneHierarchy->updateFromScene(sceneSync);
    }

    // Update result panel with animations
    if (context_->resultPanel) {
        context_->resultPanel->updateFromSync(sceneSync);
    }

    // Update editor scene
    if (context_->editorScene) {
        context_->editorScene->updateFromSync(*sceneSync);
    }

    qDebug() << "SceneUpdater: Scene updated";
    emit sceneUpdateCompleted();
}

void SceneUpdater::onEvent(const Event &event) {
    switch (event.type) {
        case EventType::SceneUpdated:
            handleSceneUpdated();
            break;
        default:
            break;
    }
}

}// namespace rbc
