#include <rbc_render/utils/heitz_sobol.h>
#include <luisa/core/logging.h>
#include <luisa/core/binary_file_stream.h>

constexpr size_t kSpp1ScramblingTileOffset = 0;
constexpr size_t kSpp2ScramblingTileOffset = 524288;
constexpr size_t kSpp4ScramblingTileOffset = 1048576;
constexpr size_t kSpp8ScramblingTileOffset = 1572864;
constexpr size_t kSpp16ScramblingTileOffset = 2097152;
constexpr size_t kSpp32ScramblingTileOffset = 2621440;
constexpr size_t kSpp64ScramblingTileOffset = 3145728;
constexpr size_t kSpp128ScramblingTileOffset = 3670016;
constexpr size_t kSpp256ScramblingTileOffset = 4194304;
constexpr size_t kSpp2RankingTileOffset = 4718592;
constexpr size_t kSpp4RankingTileOffset = 5242880;
constexpr size_t kSpp8RankingTileOffset = 5767168;
constexpr size_t kSpp16RankingTileOffset = 6291456;
constexpr size_t kSpp32RankingTileOffset = 6815744;
constexpr size_t kSpp64RankingTileOffset = 7340032;
constexpr size_t kSpp128RankingTileOffset = 7864320;
constexpr size_t kSpp256RankingTileOffset = 8388608;
constexpr size_t kSobol256dOffset = 8912896;

namespace {

luisa::compute::Buffer<uint> _load_heitz_sobol_buffer(
    luisa::string const& path,
    size_t offset,
    size_t element_count,
    luisa::compute::Device& device,
    luisa::compute::CommandList& cmdlist,
    rbc::DisposeQueue& after_commit_dspqueue) {
    luisa::BinaryFileStream file_stream(path);
    file_stream.set_pos(offset);
    luisa::vector<uint> data;
    data.push_back_uninitialized(element_count);
    file_stream.read({reinterpret_cast<std::byte*>(data.data()), data.size_bytes()});
    auto buffer = device.create_buffer<uint>(data.size());
    cmdlist << buffer.view().copy_from(luisa::span(data));
    after_commit_dspqueue.dispose_after_queue(std::move(data));
    return buffer;
}

}// namespace

luisa::compute::Buffer<uint> heitz_sobol_scrambling(
    luisa::string const& path,
    luisa::compute::Device& device,
    luisa::compute::CommandList& cmdlist,
    rbc::DisposeQueue& after_commit_dspqueue,
    HeitzSobolSPP spp) {
    auto ptr_offset = [&]() -> size_t {
        switch (spp) {
            case HeitzSobolSPP::SPP1:
                return kSpp1ScramblingTileOffset;
            case HeitzSobolSPP::SPP2:
                return kSpp2ScramblingTileOffset;
            case HeitzSobolSPP::SPP4:
                return kSpp4ScramblingTileOffset;
            case HeitzSobolSPP::SPP8:
                return kSpp8ScramblingTileOffset;
            case HeitzSobolSPP::SPP16:
                return kSpp16ScramblingTileOffset;
            case HeitzSobolSPP::SPP32:
                return kSpp32ScramblingTileOffset;
            case HeitzSobolSPP::SPP64:
                return kSpp64ScramblingTileOffset;
            case HeitzSobolSPP::SPP128:
                return kSpp128ScramblingTileOffset;
            case HeitzSobolSPP::SPP256:
                return kSpp256ScramblingTileOffset;
            default:
                LUISA_ERROR("Invalid enum.");
                return 0;
        }
    }();
    return _load_heitz_sobol_buffer(path, ptr_offset, 128 * 128 * 8, device, cmdlist, after_commit_dspqueue);
}

luisa::compute::Buffer<uint> heitz_sobol_ranking(
    luisa::string const& path,
    luisa::compute::Device& device,
    luisa::compute::CommandList& cmdlist,
    rbc::DisposeQueue& after_commit_dspqueue,
    HeitzSobolSPP spp) {
    auto ptr_offset = [&]() -> size_t {
        switch (spp) {
            case HeitzSobolSPP::SPP1:
                LUISA_ERROR("SPP1 has no ranking tile.");
                return 0;
            case HeitzSobolSPP::SPP2:
                return kSpp2RankingTileOffset;
            case HeitzSobolSPP::SPP4:
                return kSpp4RankingTileOffset;
            case HeitzSobolSPP::SPP8:
                return kSpp8RankingTileOffset;
            case HeitzSobolSPP::SPP16:
                return kSpp16RankingTileOffset;
            case HeitzSobolSPP::SPP32:
                return kSpp32RankingTileOffset;
            case HeitzSobolSPP::SPP64:
                return kSpp64RankingTileOffset;
            case HeitzSobolSPP::SPP128:
                return kSpp128RankingTileOffset;
            case HeitzSobolSPP::SPP256:
                return kSpp256RankingTileOffset;
            default:
                LUISA_ERROR("Invalid enum.");
                return 0;
        }
    }();
    return _load_heitz_sobol_buffer(path, ptr_offset, 128 * 128 * 8, device, cmdlist, after_commit_dspqueue);
}

luisa::compute::Buffer<uint> heitz_sobol_256d(
    luisa::string const& path,
    luisa::compute::Device& device,
    luisa::compute::CommandList& cmdlist,
    rbc::DisposeQueue& after_commit_dspqueue) {
    return _load_heitz_sobol_buffer(path, kSobol256dOffset, 256 * 256, device, cmdlist, after_commit_dspqueue);
}
