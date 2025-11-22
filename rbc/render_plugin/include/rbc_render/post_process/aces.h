#pragma once
#include <rbc_core/utils/curve.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/volume.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/shader.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/host_buffer_manager.h>
#include <rbc_graphics/bindless_allocator.h>
#include <rbc_render/generated/pipeline_settings.hpp>
namespace rbc
{
using namespace luisa::compute;
struct Pipeline;
struct ACES {
#include <post_process/aces_arg.inl>

private:
    Shader3D<
        Volume<float>, //& dst_volume,
        Image<float>,
        Args, // args
        bool  // disable aces
        > const* lut3d_shader;
    bool is_hdr;
    void get_curve_texture(
        ACESParameters const& desc,
        Device& device,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer
    );

public:
    Image<float> curve_img;
    Volume<float> lut3d_volume;

    ACES(luisa::fiber::counter& counter,
        bool is_hdr);
    ~ACES();
    void early_render(
        ACESParameters const& desc,
        Device& device,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer
    );
    void dispatch(
        ACESParameters const& desc,
        CommandList& cmdlist, bool disable_aces
    );
    ACES(ACES const&) = delete;
    ACES(ACES&&) = delete;
};
} // namespace rbc