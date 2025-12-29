#pragma once
#include <rbc_anim/types.h>
#include "rbc_core/serde.h"
#include "rbc_anim/graph/AnimNodeContext.h"

namespace rbc {

struct AnimOptimizationOverride {};

// The Runtime AnimSequence Asset
struct AnimSequence final {
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
    [[nodiscard]] const AnimSequenceRuntimeAsset &GetRawAnim() const { return animation; }
    [[nodiscard]] int GetNumSoaTracks() const { return animation.num_soa_tracks(); }
    [[nodiscard]] int GetNumTracks() const { return animation.num_tracks(); }

    void log_brief() const;// debug helper

    // void TickAssetPlayer(AnimTickRecord &InTickRecord, AnimAssetTickContext &InContext);
    // Actural Evaluate
    void GetAnimationPose(AnimationPoseData &OutPoseData, const AnimExtractContext &InExtractContext) const;


private:
    friend class rbc::Serialize<rbc::AnimSequence>;
    AnimSequenceRuntimeAsset animation;
    float rate_scale = 1.0f;
};

}// namespace rbc

template<>
struct rbc::Serialize<rbc::AnimSequence> {
    static RBC_RUNTIME_API bool write(rbc::ArchiveWrite &w, const rbc::AnimSequence &v);
    static RBC_RUNTIME_API bool read(rbc::ArchiveRead &r, rbc::AnimSequence &v);
};