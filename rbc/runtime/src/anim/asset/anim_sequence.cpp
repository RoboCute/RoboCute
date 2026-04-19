/**
 * Raw AnimSequence Asset
 * Wraps the IO of ozz assets
 */

#include "rbc_anim/asset/anim_sequence.h"
#include "rbc_anim/asset/ozz_stream.h"
#include <ozz/base/io/archive.h>

namespace rbc::world {

AnimSequence::AnimSequence() {}
AnimSequence::~AnimSequence() {}
AnimSequence::AnimSequence(AnimSequence &&Other) {
    _animation = std::move(Other._animation);
}
AnimSequence &AnimSequence::operator=(AnimSequence &&Other) {
    _animation = std::move(Other._animation);
    return *this;
}
AnimSequence::AnimSequence(AnimSequenceRuntimeAsset &&InAnim) {
    _animation = std::move(InAnim);
}
AnimSequence &AnimSequence::operator=(AnimSequenceRuntimeAsset &&InAnim) {
    _animation = std::move(InAnim);
    return *this;
}

void AnimSequence::log_brief() const {
    LUISA_INFO("Anim has {} Tracks {} soa tracks {} timepoints of duration {}",
               _animation.num_tracks(),
               _animation.num_soa_tracks(),
               _animation.timepoints().size(),
               _animation.duration());
}

}// namespace rbc::world

namespace rbc {
bool rbc::Serialize<rbc::world::AnimSequence>::write(rbc::ArchiveWrite &w, const rbc::world::AnimSequence &v) {
    // Use OzzStream in write mode - buffers all data internally
    OzzStream ozz_stream;
    ozz::io::OArchive archive(&ozz_stream);
    archive << v._animation;

    // Write the buffered data as a single bytes field
    auto buffer = ozz_stream.buffer();
    w.bytes(buffer, "data");
    return true;
}

bool rbc::Serialize<rbc::world::AnimSequence>::read(rbc::ArchiveRead &r, rbc::world::AnimSequence &v) {
    // Read the entire bytes blob first
    luisa::vector<std::byte> data;
    if (!r.bytes(data, "data")) {
        LUISA_ERROR("Failed to read animation");
    }
    // Use OzzStream in read mode - provides sequential read from buffer
    OzzStream ozz_stream(luisa::span<const std::byte>{data.data(), data.size()});
    ozz::io::IArchive archive(&ozz_stream);
    archive >> v._animation;

    return true;
}

}// namespace rbc