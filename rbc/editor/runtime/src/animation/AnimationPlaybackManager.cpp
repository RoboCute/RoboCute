#include "RBCEditorRuntime/runtime/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include <QTimer>
#include <luisa/core/logging.h>

namespace rbc {

AnimationPlaybackManager::AnimationPlaybackManager(EditorScene *scene, QObject *parent)
    : QObject(parent),
      scene_(scene),
      currentClip_(nullptr),
      currentFrame_(0),
      isPlaying_(false) {

    playbackTimer_ = new QTimer(this);
    connect(playbackTimer_, &QTimer::timeout, this, &AnimationPlaybackManager::onTimerTick);
}

void AnimationPlaybackManager::setAnimation(const AnimationClip *clip) {
    if (isPlaying_) {
        pause();
    }

    currentClip_ = clip;
    currentFrame_ = 0;

    if (clip && clip->fps > 0) {
        int intervalMs = static_cast<int>(1000.0f / clip->fps);
        playbackTimer_->setInterval(intervalMs);

        LUISA_INFO("AnimationPlaybackManager: Loaded animation '{}', {} frames at {} fps, {} sequence(s)",
                   clip->name.c_str(), clip->total_frames, clip->fps, clip->sequences.size());
    }
}

void AnimationPlaybackManager::clearAnimation() {
    pause();
    currentClip_ = nullptr;
    currentFrame_ = 0;
}

void AnimationPlaybackManager::play() {
    if (!currentClip_ || currentClip_->total_frames <= 0) {
        return;
    }

    isPlaying_ = true;
    playbackTimer_->start();

    LUISA_INFO("AnimationPlaybackManager: Playing animation '{}'", currentClip_->name.c_str());
}

void AnimationPlaybackManager::pause() {
    isPlaying_ = false;
    playbackTimer_->stop();
}

void AnimationPlaybackManager::stop() {
    pause();
    currentFrame_ = 0;
    applyFrame(0);
}

void AnimationPlaybackManager::setFrame(int frame) {
    if (!currentClip_) return;

    if (frame < 0) frame = 0;
    if (frame > currentClip_->total_frames) frame = currentClip_->total_frames;

    currentFrame_ = frame;
    applyFrame(frame);

    emit frameChanged(frame);
}

void AnimationPlaybackManager::onTimerTick() {
    if (!isPlaying_ || !currentClip_) return;

    currentFrame_++;

    // Loop animation
    if (currentFrame_ > currentClip_->total_frames) {
        currentFrame_ = 0;
        emit playbackFinished();
    }

    applyFrame(currentFrame_);
    emit frameChanged(currentFrame_);
}

void AnimationPlaybackManager::applyFrame(int frame) {
    if (!currentClip_ || !scene_) return;

    // Apply transforms from all sequences to scene entities
    for (const auto &seq : currentClip_->sequences) {
        // Sample keyframe at current frame
        AnimationKeyframe kf;
        sampleKeyframe(seq, frame, kf);

        // Create transform and apply to scene
        Transform transform;
        transform.position = luisa::float3(kf.position[0], kf.position[1], kf.position[2]);
        transform.rotation = luisa::float4(kf.rotation[0], kf.rotation[1], kf.rotation[2], kf.rotation[3]);
        transform.scale = luisa::float3(kf.scale[0], kf.scale[1], kf.scale[2]);

        // Apply to scene (EditorScene needs this method - will add it)
        scene_->setAnimationTransform(seq.entity_id, transform);
    }
}

void AnimationPlaybackManager::sampleKeyframe(const AnimationSequence &seq, int frame, AnimationKeyframe &out) {
    // Find exact keyframe or nearest keyframe
    const AnimationKeyframe *exactKf = seq.getKeyframeAtFrame(frame);

    if (exactKf) {
        // Exact match
        out = *exactKf;
        return;
    }

    // No interpolation for now - find nearest keyframe
    if (seq.keyframes.empty()) {
        // Return identity
        out.frame = frame;
        out.position[0] = out.position[1] = out.position[2] = 0.0f;
        out.rotation[0] = out.rotation[1] = out.rotation[2] = 0.0f;
        out.rotation[3] = 1.0f;
        out.scale[0] = out.scale[1] = out.scale[2] = 1.0f;
        return;
    }

    // Find closest keyframe
    const AnimationKeyframe *closest = &seq.keyframes[0];
    int minDist = abs(seq.keyframes[0].frame - frame);

    for (const auto &kf : seq.keyframes) {
        int dist = abs(kf.frame - frame);
        if (dist < minDist) {
            minDist = dist;
            closest = &kf;
        }
    }

    out = *closest;
    out.frame = frame;
}

}// namespace rbc
