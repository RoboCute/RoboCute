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

}// namespace rbc