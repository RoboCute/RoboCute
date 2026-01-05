#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/lights.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <luisa/core/clock.h>
#include <luisa/vstl/functional.h>
#include <luisa/runtime/swapchain.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
namespace rbc {
namespace world {
struct MeshResource;
}// namespace world
using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>
struct DeviceTransformingMesh;
struct EventFence {
    TimelineEvent event;
    uint64_t fence_index{};
};
struct RBC_RUNTIME_API GraphicsUtils {
private:
    RenderDevice _render_device;
    luisa::string _backend_name;
    EventFence _compute_event;
    vstd::optional<SceneManager> _sm;
    // present
    Stream _present_stream;
    Swapchain _swapchain;
    Image<float> _dst_image;
    DenoisePack _denoise_pack;
    // render
    luisa::shared_ptr<DynamicModule> _render_module;
    RenderPlugin *_render_plugin{};
    StateMap *_render_settings;
    RenderPlugin::PipeCtxStub *_display_pipe_ctx{};
    vstd::optional<rbc::Lights> _lights;
    bool _require_reset : 1 {false};
    bool _denoiser_inited : 1 {false};

public:
    auto render_plugin() const { return _render_plugin; }
    auto &present_stream() const { return _present_stream; }
    auto &render_settings() const { return *_render_settings; }
    auto &render_settings() { return *_render_settings; }
    auto default_pipe_ctx() const { return _display_pipe_ctx; }
    auto &dst_image() const { return _dst_image; }
    auto &backend_name() const { return _backend_name; }
    GraphicsUtils();
    void dispose(vstd::function<void()> after_sync = {});
    ~GraphicsUtils();
    void init_device(luisa::string_view program_path, luisa::string_view backend_name);
    void init_graphics(luisa::filesystem::path const &shader_path);
    void init_present_stream();
    void init_render();
    void resize_swapchain(
        uint2 size,
        uint64_t native_display,
        uint64_t native_handle);

    void init_display(
        uint2 resolution,
        uint64_t native_display,
        uint64_t native_handle);
    void reset_frame();
    enum struct TickStage {
        RasterPreview,
        PathTracingPreview,
        OffineCapturing,
        PresentOfflineResult
    };
    void build_mesh(DeviceMesh *mesh);
    void build_transforming_mesh(DeviceTransformingMesh *mesh);
    void tick(
        float delta_time,
        uint64_t frame_index,
        uint2 resolution,
        TickStage tick_stage = TickStage::PathTracingPreview,
        bool enable_denoise = false);
    void denoise();
    void create_texture(
        DeviceImage *ptr,
        PixelStorage storage,
        uint2 size, uint mip_level);
    void update_texture(
        DeviceImage *ptr);
    void create_mesh(
        DeviceMesh *ptr,
        uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count, vstd::vector<uint> &&offsets);
    void update_mesh_data(DeviceMesh *mesh, bool only_vertex);
    void update_skinning(
        world::MeshResource *skinning_mesh,
        BufferView<DualQuaternion> bones);
};
}// namespace rbc