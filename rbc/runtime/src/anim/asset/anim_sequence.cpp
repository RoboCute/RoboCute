#include "rbc_anim/asset/anim_sequence.h"

namespace rbc {

AnimSequence::AnimSequence() {}
AnimSequence::~AnimSequence() {}
AnimSequence::AnimSequence(AnimSequence &&Other) {
    animation = std::move(Other.animation);
}
AnimSequence &AnimSequence::operator=(AnimSequence &&Other) {
    animation = std::move(Other.animation);
    return *this;
}
AnimSequence::AnimSequence(AnimSequenceRuntimeAsset &&InAnim) {
    animation = std::move(InAnim);
}
AnimSequence &AnimSequence::operator=(AnimSequenceRuntimeAsset &&InAnim) {
    animation = std::move(InAnim);
    return *this;
}

void AnimSequence::log_brief() const {
    LUISA_INFO("Anim has {} Tracks {} soa tracks {} timepoints of duration {}", 
        animation.num_tracks(), 
        animation.num_soa_tracks(), 
        animation.timepoints().size(), 
        animation.duration());
}

}// namespace rbc