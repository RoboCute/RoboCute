#pragma once

#include <vector>
#include <string>
#include <luisa/vstl/common.h>

namespace rbc {

/**
 * Animation keyframe data
 */
struct AnimationKeyframe {
    int frame = 0;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};// quaternion (x, y, z, w)
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

/**
 * Animation sequence for a single entity
 */
struct AnimationSequence {
    int entity_id = 0;
    luisa::vector<AnimationKeyframe> keyframes;

    // Get total frames in this sequence
    int getTotalFrames() const {
        if (keyframes.empty()) return 0;
        int max_frame = 0;
        for (const auto &kf : keyframes) {
            if (kf.frame > max_frame) max_frame = kf.frame;
        }
        return max_frame;
    }

    // Get keyframe at specific frame (exact match, or nullptr if not found)
    const AnimationKeyframe *getKeyframeAtFrame(int frame) const {
        for (const auto &kf : keyframes) {
            if (kf.frame == frame) return &kf;
        }
        return nullptr;
    }
};

/**
 * Animation clip containing multiple entity sequences
 */
struct AnimationClip {
    luisa::string name;
    luisa::vector<AnimationSequence> sequences;
    float fps = 30.0f;
    int total_frames = 0;
    float duration_seconds = 0.0f;

    // Get sequence for specific entity
    const AnimationSequence *getSequenceForEntity(int entity_id) const {
        for (const auto &seq : sequences) {
            if (seq.entity_id == entity_id) return &seq;
        }
        return nullptr;
    }

    // Get total frames across all sequences
    int getTotalFrames() const {
        if (sequences.empty()) return 0;
        int max_frames = 0;
        for (const auto &seq : sequences) {
            int seq_frames = seq.getTotalFrames();
            if (seq_frames > max_frames) max_frames = seq_frames;
        }
        return max_frames;
    }
};

}// namespace rbc
