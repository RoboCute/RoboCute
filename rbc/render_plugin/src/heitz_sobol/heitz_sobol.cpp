#include <rbc_render/utils/heitz_sobol.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/logging.h>
// export heitz-sobol binary data
/*
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

LUISA_EXPORT_API void save_heitz_sobol_to_file(luisa::filesystem::path const &path) {
    BinaryFileWriter writer{luisa::to_string(path)};
    size_t offset = 0;
    auto write_file = [&](int *ptr, size_t size_bytes) {
        auto local_offset = offset;
        offset += size_bytes;
        writer.write({(std::byte *)ptr,
                      size_bytes});
        return local_offset;
    };
#define WRITE_ARRAY(x)                                         \
    {                                                          \
        auto offset = write_file(x, vstd::array_byte_size(x)); \
        auto str = luisa::format("constexpr size_t " #x        \
                                 "_offset = {};\n",              \
                                 offset);                      \
        printf(str.c_str());                                   \
    }
    WRITE_ARRAY(spp1_scramblingTile);
    WRITE_ARRAY(spp2_scramblingTile);
    WRITE_ARRAY(spp4_scramblingTile);
    WRITE_ARRAY(spp8_scramblingTile);
    WRITE_ARRAY(spp16_scramblingTile);
    WRITE_ARRAY(spp32_scramblingTile);
    WRITE_ARRAY(spp64_scramblingTile);
    WRITE_ARRAY(spp128_scramblingTile);
    WRITE_ARRAY(spp256_scramblingTile);
    WRITE_ARRAY(spp2_rankingTile);
    WRITE_ARRAY(spp4_rankingTile);
    WRITE_ARRAY(spp8_rankingTile);
    WRITE_ARRAY(spp16_rankingTile);
    WRITE_ARRAY(spp32_rankingTile);
    WRITE_ARRAY(spp64_rankingTile);
    WRITE_ARRAY(spp128_rankingTile);
    WRITE_ARRAY(spp256_rankingTile);
    WRITE_ARRAY(sobol_256d);
#undef WRITE_ARRAY
}
*/
constexpr size_t spp1_scramblingTile_offset = 0;
constexpr size_t spp2_scramblingTile_offset = 524288;
constexpr size_t spp4_scramblingTile_offset = 1048576;
constexpr size_t spp8_scramblingTile_offset = 1572864;
constexpr size_t spp16_scramblingTile_offset = 2097152;
constexpr size_t spp32_scramblingTile_offset = 2621440;
constexpr size_t spp64_scramblingTile_offset = 3145728;
constexpr size_t spp128_scramblingTile_offset = 3670016;
constexpr size_t spp256_scramblingTile_offset = 4194304;
constexpr size_t spp2_rankingTile_offset = 4718592;
constexpr size_t spp4_rankingTile_offset = 5242880;
constexpr size_t spp8_rankingTile_offset = 5767168;
constexpr size_t spp16_rankingTile_offset = 6291456;
constexpr size_t spp32_rankingTile_offset = 6815744;
constexpr size_t spp64_rankingTile_offset = 7340032;
constexpr size_t spp128_rankingTile_offset = 7864320;
constexpr size_t spp256_rankingTile_offset = 8388608;
constexpr size_t sobol_256d_offset = 8912896;

luisa::compute::Buffer<uint> heitz_sobol_scrambling(
    luisa::string const& path,
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    rbc::DisposeQueue &after_commit_dspqueue,
    HeitzSobolSPP spp) {
    auto ptr_offset = [&]() -> size_t {
        switch (spp) {
            case HeitzSobolSPP::SPP1:
                return spp1_scramblingTile_offset;
            case HeitzSobolSPP::SPP2:
                return spp2_scramblingTile_offset;
            case HeitzSobolSPP::SPP4:
                return spp4_scramblingTile_offset;
            case HeitzSobolSPP::SPP8:
                return spp8_scramblingTile_offset;
            case HeitzSobolSPP::SPP16:
                return spp16_scramblingTile_offset;
            case HeitzSobolSPP::SPP32:
                return spp32_scramblingTile_offset;
            case HeitzSobolSPP::SPP64:
                return spp64_scramblingTile_offset;
            case HeitzSobolSPP::SPP128:
                return spp128_scramblingTile_offset;
            case HeitzSobolSPP::SPP256:
                return spp256_scramblingTile_offset;
            default:
                LUISA_ERROR("Invalid enum.");
                return 0;
        }
    }();
    luisa::BinaryFileStream file_stream(
        path);
    file_stream.set_pos(ptr_offset);
    luisa::vector<uint> data;
    data.push_back_uninitialized(128 * 128 * 8);
    file_stream.read(
        {(std::byte *)data.data(),
         data.size_bytes()});

    auto buffer = device.create_buffer<uint>(data.size());
    cmdlist << buffer.view().copy_from(data.data());
    after_commit_dspqueue.dispose_after_queue(std::move(data));
    return buffer;
}
luisa::compute::Buffer<uint> heitz_sobol_ranking(
    luisa::string const& path,
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    rbc::DisposeQueue &after_commit_dspqueue,
    HeitzSobolSPP spp) {
    auto ptr_offset = [&]() -> size_t {
        switch (spp) {
            case HeitzSobolSPP::SPP1:
                LUISA_ERROR("SPP1 has no ranking tile.");
                return 0;
            case HeitzSobolSPP::SPP2:
                return spp2_rankingTile_offset;
            case HeitzSobolSPP::SPP4:
                return spp4_rankingTile_offset;
            case HeitzSobolSPP::SPP8:
                return spp8_rankingTile_offset;
            case HeitzSobolSPP::SPP16:
                return spp16_rankingTile_offset;
            case HeitzSobolSPP::SPP32:
                return spp32_rankingTile_offset;
            case HeitzSobolSPP::SPP64:
                return spp64_rankingTile_offset;
            case HeitzSobolSPP::SPP128:
                return spp128_rankingTile_offset;
            case HeitzSobolSPP::SPP256:
                return spp256_rankingTile_offset;
            default:
                LUISA_ERROR("Invalid enum.");
                return 0;
        }
    }();
    luisa::BinaryFileStream file_stream(
        path);
    file_stream.set_pos(ptr_offset);
    luisa::vector<uint> data;
    data.push_back_uninitialized(128 * 128 * 8);
    file_stream.read(
        {(std::byte *)data.data(),
         data.size_bytes()});
    auto buffer = device.create_buffer<uint>(data.size());
    cmdlist << buffer.view().copy_from(data.data());
    after_commit_dspqueue.dispose_after_queue(std::move(data));
    return buffer;
}
luisa::compute::Buffer<uint> heitz_sobol_256d(
    luisa::string const& path,
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    rbc::DisposeQueue &after_commit_dspqueue) {
    luisa::BinaryFileStream file_stream(
        path);
    file_stream.set_pos(sobol_256d_offset);
    luisa::vector<uint> data;
    data.push_back_uninitialized(256 * 256);
    file_stream.read(
        {(std::byte *)data.data(),
         data.size_bytes()});
    auto buffer = device.create_buffer<uint>(data.size());
    cmdlist << buffer.view().copy_from(data.data());
    after_commit_dspqueue.dispose_after_queue(std::move(data));
    return buffer;
}