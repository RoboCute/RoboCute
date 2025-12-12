#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include "RBCEditorRuntime/animation/AnimationClip.h"

namespace rbc {

class EditorScene;
class SceneSync;

/**
 * Animation Playback Manager
 * 
 * Manages animation playback by:
 * - Loading animation clips
 * - Sampling keyframes at given frames
 * - Applying transforms to EditorScene entities
 * - Managing playback loop with timer
 */
class AnimationPlaybackManager : public QObject {
    Q_OBJECT

public:
    explicit AnimationPlaybackManager(EditorScene *scene, QObject *parent = nullptr);

    // Load animation clip for playback
    void setAnimation(const AnimationClip *clip);

    // Clear current animation
    void clearAnimation();

    // Playback control
    void play();
    void pause();
    void stop();
    [[nodiscard]] bool isPlaying() const { return isPlaying_; }

    // Frame control
    void setFrame(int frame);
    [[nodiscard]] inline int currentFrame() const { return currentFrame_; }

signals:
    void frameChanged(int frame);
    void playbackFinished();

private slots:
    void onTimerTick();

private:
    void applyFrame(int frame);
    static void sampleKeyframe(const AnimationSequence &seq, int frame, AnimationKeyframe &out);

    EditorScene *scene_;
    const AnimationClip *currentClip_;

    int currentFrame_;
    bool isPlaying_;
    QTimer *playbackTimer_;
};

}// namespace rbc
