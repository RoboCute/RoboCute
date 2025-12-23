#pragma once
#include <rbc_anim/types.h>

namespace rbc {

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

private:
    AnimSequenceRuntimeAsset animation;
    float rate_scale = 1.0f;
};

}// namespace rbc
