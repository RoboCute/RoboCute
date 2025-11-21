#pragma once
#include <rbc_config.h>
#include <rbc_graphics/camera.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/rhi/command.h>
#include <luisa/runtime/image.h>
namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
struct  FrameGen {
private:
    void* _impl{ nullptr };

public:
    FrameGen(
        filesystem::path const& path,
        Device& device,
        uint2 render_resolution,
        uint2 display_resolution,
        float minLuminance, float maxLuminance,
        PixelStorage dst_storage
    );

    unique_ptr<Command> execute_frame_gen(
        Image<float> const& in_frame,
        Image<float> const& out_frame
    );

    unique_ptr<Command> execute_upscale(
        Camera const& cam,
        float2 jitter,
        float delta_time,
        float sharpness,
        Image<float> const& unresolved_img,
        Image<float>&& reactive_img,
        Image<float>&& motionvec_img,
        Image<float>&& depth_img,
        Image<float> const& resolved_img,
        bool reset
    );

    struct CameraSetup {
        float near_plane;
        float far_plane;
        float fov;
        bool reset;
    };
    struct UploadSetup {
        CameraSetup cam;
        Image<float> const* unresolved_color_resource;      // input0
        Image<float> motionvector_resource;                 // input1
        Image<float> depthbuffer_resource;                  // input2
        Image<float> reactive_map_resource;                 // input3
        Image<float> transparency_and_composition_resource; // input4
        Image<float> const* resolved_color_resource;        // output
    };
    std::pair<float2, uint> get_jitter_and_phase_count(uint frame_index);

    ~FrameGen();
};
} // namespace rbc