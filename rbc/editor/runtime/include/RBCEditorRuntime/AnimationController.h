#pragma once

#include <QObject>
#include <QString>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/EventBus.h"

namespace rbc {

class EditorContext;
class AnimationPlayer;
class AnimationPlaybackManager;
class SceneSyncManager;

/**
 * AnimationController - 处理动画播放控制相关的逻辑
 * 
 * 职责：
 * - 响应动画选择事件，更新播放器和场景显示
 * - 响应动画帧变化事件，更新播放器和场景显示
 * - 管理动画播放状态
 */
class RBC_EDITOR_RUNTIME_API AnimationController : public QObject, public IEventSubscriber {
    Q_OBJECT

public:
    explicit AnimationController(EditorContext *context, QObject *parent = nullptr);
    ~AnimationController() override;

    /**
     * 初始化控制器，订阅相关事件
     */
    void initialize();

    /**
     * 处理动画选择
     * @param animName 动画名称
     */
    void handleAnimationSelected(const QString &animName);

    /**
     * 处理动画帧变化
     * @param frame 帧号
     */
    void handleAnimationFrameChanged(int frame);

    /**
     * IEventSubscriber 接口实现
     */
    void onEvent(const Event &event) override;

signals:
    /**
     * 当动画加载完成时发出
     */
    void animationLoaded(const QString &animName);

    /**
     * 当动画加载失败时发出
     */
    void animationLoadFailed(const QString &animName, const QString &reason);

private:
    EditorContext *context_;
    int animationSelectedSubscriptionId_;
    int animationFrameChangedSubscriptionId_;
};

}// namespace rbc
