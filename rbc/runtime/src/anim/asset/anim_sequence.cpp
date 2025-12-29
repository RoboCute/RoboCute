#include "rbc_anim/asset/anim_sequence.h"
#include "rbc_anim/asset/ozz_stream.h"
#include <ozz/base/io/archive.h>

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

bool rbc::Serialize<rbc::AnimSequence>::write(rbc::ArchiveWrite &w, const rbc::AnimSequence &v) {
    // Use OzzStream in write mode - buffers all data internally
    OzzStream ozz_stream;
    ozz::io::OArchive archive(&ozz_stream);
    archive << v.animation;

    // Write the buffered data as a single bytes field
    auto buffer = ozz_stream.buffer();
    w.bytes(buffer, "data");
    return true;
}

bool rbc::Serialize<rbc::AnimSequence>::read(rbc::ArchiveRead &r, rbc::AnimSequence &v) {
    // Read the entire bytes blob first
    luisa::vector<std::byte> data;
    if (!r.bytes(data, "data")) {
        LUISA_ERROR("Failed to read animation");
    }
    // Use OzzStream in read mode - provides sequential read from buffer
    OzzStream ozz_stream(luisa::span<const std::byte>{data.data(), data.size()});
    ozz::io::IArchive archive(&ozz_stream);
    archive >> v.animation;

    return true;
}

}// namespace rbc