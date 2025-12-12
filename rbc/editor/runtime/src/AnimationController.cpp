#include "RBCEditorRuntime/AnimationController.h"
#include "RBCEditorRuntime/EditorContext.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"
#include "RBCEditorRuntime/animation/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/SceneSync.h"
#include "RBCEditorRuntime/EventBus.h"
#include <QDebug>

namespace rbc {

AnimationController::AnimationController(EditorContext *context, QObject *parent)
    : QObject(parent),
      context_(context),
      animationSelectedSubscriptionId_(-1),
      animationFrameChangedSubscriptionId_(-1) {
    Q_ASSERT(context_ != nullptr);
}

AnimationController::~AnimationController() {
    // 取消订阅事件总线
    if (animationSelectedSubscriptionId_ >= 0) {
        EventBus::instance().unsubscribe(animationSelectedSubscriptionId_);
    }
    if (animationFrameChangedSubscriptionId_ >= 0) {
        EventBus::instance().unsubscribe(animationFrameChangedSubscriptionId_);
    }
}

void AnimationController::initialize() {
    // 订阅动画选择事件
    animationSelectedSubscriptionId_ = EventBus::instance().subscribe(
        EventType::AnimationSelected,
        [this](const Event &event) {
            onEvent(event);
        });

    // 订阅动画帧变化事件
    animationFrameChangedSubscriptionId_ = EventBus::instance().subscribe(
        EventType::AnimationFrameChanged,
        [this](const Event &event) {
            onEvent(event);
        });
}

void AnimationController::handleAnimationSelected(const QString &animName) {
    if (!context_->sceneSyncManager) {
        qWarning() << "AnimationController: SceneSyncManager is not available";
        emit animationLoadFailed(animName, "SceneSyncManager is not available");
        return;
    }

    const auto *sceneSync = context_->sceneSyncManager->sceneSync();
    if (!sceneSync) {
        qWarning() << "AnimationController: SceneSync is not available";
        emit animationLoadFailed(animName, "SceneSync is not available");
        return;
    }

    // 从 SceneSync 获取动画数据
    luisa::string anim_name{animName.toStdString()};
    const auto *anim = sceneSync->getAnimation(anim_name);

    if (!anim) {
        qWarning() << "AnimationController: Animation not found:" << animName;
        emit animationLoadFailed(animName, "Animation not found");
        return;
    }

    // 更新 AnimationPlayer
    if (context_->animationPlayer) {
        context_->animationPlayer->setAnimation(animName, anim->total_frames, anim->fps);
    } else {
        qWarning() << "AnimationController: AnimationPlayer is not available";
    }

    // 更新 AnimationPlaybackManager
    if (context_->playbackManager) {
        context_->playbackManager->setAnimation(anim);
    } else {
        qWarning() << "AnimationController: AnimationPlaybackManager is not available";
    }

    qDebug() << "AnimationController: Loaded animation:" << animName
             << "frames:" << anim->total_frames << "fps:" << anim->fps;
    emit animationLoaded(animName);
}

void AnimationController::handleAnimationFrameChanged(int frame) {
    // 更新播放管理器以应用变换到场景
    if (context_->playbackManager) {
        context_->playbackManager->setFrame(frame);
    }

    qDebug() << "AnimationController: Frame changed to" << frame;
}

void AnimationController::onEvent(const Event &event) {
    switch (event.type) {
        case EventType::AnimationSelected: {
            // 从事件数据中获取动画名称
            QString animName = event.data.toString();
            if (!animName.isEmpty()) {
                handleAnimationSelected(animName);
            }
            break;
        }
        case EventType::AnimationFrameChanged: {
            // 从事件数据中获取帧号
            bool ok = false;
            int frame = event.data.toInt(&ok);
            if (ok) {
                handleAnimationFrameChanged(frame);
            }
            break;
        }
        default:
            break;
    }
}

}// namespace rbc
