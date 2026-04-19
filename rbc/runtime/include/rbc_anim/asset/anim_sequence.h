#pragma once
#include <rbc_anim/types.h>
#include "rbc_core/serde.h"
#include "rbc_anim/graph/AnimNodeContext.h"

namespace rbc::world {

struct AnimOptimizationOverride {};

// The Runtime AnimSequence Asset
struct RBC_RUNTIME_API AnimSequence final {
public:
    AnimSequence();
    ~AnimSequence();
    AnimSequence(const AnimSequence &Other) = delete;
    AnimSequence &operator=(const AnimSequence &Other) = delete;
    AnimSequence(AnimSequence &&Other);
    AnimSequence &operator=(AnimSequence &&Other);
    // move from raw
    explicit AnimSequence(AnimSequenceRuntimeAsset &&InAnim);
    AnimSequence &operator=(AnimSequenceRuntimeAsset &&InAnim);


public:
    [[nodiscard]] const AnimSequenceRuntimeAsset &GetRawAnim() const { return _animation; }
    [[nodiscard]] int GetNumSoaTracks() const { return _animation.num_soa_tracks(); }
    [[nodiscard]] int GetNumTracks() const { return _animation.num_tracks(); }

    void log_brief() const;// debug helper

    // Rate scale control
    [[nodiscard]] float GetRateScale() const { return _rate_scale; }
    void SetRateScale(float InRateScale) { _rate_scale = InRateScale; }

    // void TickAssetPlayer(AnimTickRecord &InTickRecord, AnimAssetTickContext &InContext);
    // Actural Evaluate
    void GetAnimationPose(AnimationPoseData &OutPoseData, const AnimExtractContext &InExtractContext) const;


private:
    friend struct rbc::Serialize<AnimSequence>;
    AnimSequenceRuntimeAsset _animation;
    float _rate_scale = 1.0f;
};

}// namespace rbc::world

template<>
struct rbc::Serialize<rbc::world::AnimSequence> {
    static RBC_RUNTIME_API bool write(rbc::ArchiveWrite &w, const rbc::world::AnimSequence &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::world::AnimSequence &v);
};