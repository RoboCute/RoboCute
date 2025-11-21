#include <rbc_render/utils/heitz_sobol.h>
#include <luisa/core/logging.h>
extern int spp1_scramblingTile[128 * 128 * 8];
extern int spp2_scramblingTile[128 * 128 * 8];
extern int spp4_scramblingTile[128 * 128 * 8];
extern int spp8_scramblingTile[128 * 128 * 8];
extern int spp16_scramblingTile[128 * 128 * 8];
extern int spp32_scramblingTile[128 * 128 * 8];
extern int spp64_scramblingTile[128 * 128 * 8];
extern int spp128_scramblingTile[128 * 128 * 8];
extern int spp256_scramblingTile[128 * 128 * 8];
extern int spp2_rankingTile[128 * 128 * 8];
extern int spp4_rankingTile[128 * 128 * 8];
extern int spp8_rankingTile[128 * 128 * 8];
extern int spp16_rankingTile[128 * 128 * 8];
extern int spp32_rankingTile[128 * 128 * 8];
extern int spp64_rankingTile[128 * 128 * 8];
extern int spp128_rankingTile[128 * 128 * 8];
extern int spp256_rankingTile[128 * 128 * 8];
extern int sobol_256d[256 * 256];

luisa::compute::Buffer<uint> heitz_sobol_scrambling(
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    HeitzSobolSPP spp) {
    void *ptr = [&]() -> void * {
        switch (spp) {
            case HeitzSobolSPP::SPP1:
                return spp1_scramblingTile;
            case HeitzSobolSPP::SPP2:
                return spp2_scramblingTile;
            case HeitzSobolSPP::SPP4:
                return spp4_scramblingTile;
            case HeitzSobolSPP::SPP8:
                return spp8_scramblingTile;
            case HeitzSobolSPP::SPP16:
                return spp16_scramblingTile;
            case HeitzSobolSPP::SPP32:
                return spp32_scramblingTile;
            case HeitzSobolSPP::SPP64:
                return spp64_scramblingTile;
            case HeitzSobolSPP::SPP128:
                return spp128_scramblingTile;
            case HeitzSobolSPP::SPP256:
                return spp256_scramblingTile;
            default:
                LUISA_ERROR("Invalid enum.");
                return nullptr;
        }
    }();
    auto buffer = device.create_buffer<uint>(128 * 128 * 8);
    cmdlist << buffer.view().copy_from(ptr);
    return buffer;
}
luisa::compute::Buffer<uint> heitz_sobol_ranking(
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    HeitzSobolSPP spp) {
    void *ptr = [&]() -> void * {
        switch (spp) {
            case HeitzSobolSPP::SPP1:
                LUISA_ERROR("SPP1 has no ranking tile.");
                return nullptr;
            case HeitzSobolSPP::SPP2:
                return spp2_rankingTile;
            case HeitzSobolSPP::SPP4:
                return spp4_rankingTile;
            case HeitzSobolSPP::SPP8:
                return spp8_rankingTile;
            case HeitzSobolSPP::SPP16:
                return spp16_rankingTile;
            case HeitzSobolSPP::SPP32:
                return spp32_rankingTile;
            case HeitzSobolSPP::SPP64:
                return spp64_rankingTile;
            case HeitzSobolSPP::SPP128:
                return spp128_rankingTile;
            case HeitzSobolSPP::SPP256:
                return spp256_rankingTile;
            default:
                LUISA_ERROR("Invalid enum.");
                return nullptr;
        }
    }();
    auto buffer = device.create_buffer<uint>(128 * 128 * 8);
    cmdlist << buffer.view().copy_from(ptr);
    return buffer;
}
luisa::compute::Buffer<uint> heitz_sobol_256d(
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist) {
    auto buffer = device.create_buffer<uint>(vstd::array_count(sobol_256d));
    cmdlist << buffer.view().copy_from(sobol_256d);
    return buffer;
}