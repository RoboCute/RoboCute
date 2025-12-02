#pragma once
#include <rbc_config.h>
#include <rbc_graphics/device_assets/device_resource.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/sparse_image.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
struct TexStreamManager;
struct RBC_RUNTIME_API DeviceSparseImage : DeviceResource {
private:
    SparseImage<float> const* _sparse_img{ nullptr };
    uint _heap_idx{ ~0u };
    TexStreamManager* _tex_stream_mng;
    std::atomic<void*> _tex_idx = nullptr;

public:
    bool load_finished() const override;
    Type resource_type() const override { return Type::SparseImage; }
    [[nodiscard]] auto heap_idx() const { return _heap_idx; }
    DeviceSparseImage();
    ~DeviceSparseImage();
    void load(
        TexStreamManager* tex_stream,
        luisa::move_only_function<void()>&& init_callback,
        luisa::filesystem::path const& path,
        uint64_t file_offset,
        Sampler sampler,
        PixelStorage storage,
        uint2 size,
        uint mip_level = 1u
    );
};
} // namespace rbc