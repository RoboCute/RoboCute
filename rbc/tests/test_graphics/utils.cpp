#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_runtime/render_plugin.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>
#include <luisa/runtime/rtx/triangle.h>
void warm_up_accel() {
    using namespace luisa;
    using namespace luisa::compute;
    auto &render_device = rbc::RenderDevice::instance();

    // Build a simple accel to preload driver builtin shaders
    auto buffer = render_device.lc_device().create_buffer<uint>(4 * 3 + 3);
    auto vb = buffer.view(0, 4 * 3).as<float3>();
    auto ib = buffer.view(4 * 3, 3).as<Triangle>();
    uint tri[] = {0, 1, 2};
    float data[] = {
        0, 0, 0, 0,
        0, 1, 0, 0,
        1, 1, 0, 0,
        reinterpret_cast<float &>(tri[0]),
        reinterpret_cast<float &>(tri[1]),
        reinterpret_cast<float &>(tri[2])};
    auto mesh = render_device.lc_device().create_mesh(vb, ib, AccelOption{.hint = AccelUsageHint::FAST_BUILD, .allow_compaction = false});
    auto accel = render_device.lc_device().create_accel(AccelOption{.hint = AccelUsageHint::FAST_BUILD, .allow_compaction = false});
    accel.emplace_back(mesh);
    render_device.lc_main_stream()
        << buffer.view().copy_from(data)
        << mesh.build()
        << accel.build()
        << [accel = std::move(accel), mesh = std::move(mesh), buffer = std::move(buffer)]() {};
}

void before_frame(rbc::RenderPlugin *render_plugin, rbc::RenderPlugin::PipeCtxStub *pipe_ctx) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    render_device.render_loop_mtx().lock();
    auto &cmdlist = render_device.lc_main_cmd_list();
    auto &main_stream = render_device.lc_main_stream();
    render_plugin->before_rendering({}, pipe_ctx);
    sm.before_rendering(
        cmdlist,
        main_stream);
    // on render
    auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
    managed_device->begin_managing(cmdlist);
}

void after_frame(
    rbc::RenderPlugin *render_plugin,
    rbc::RenderPlugin::PipeCtxStub *pipe_ctx,
    luisa::compute::TimelineEvent *timeline_event,
    uint64_t signal_fence) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto managed_device = static_cast<ManagedDevice *>(RenderDevice::instance()._lc_managed_device().impl());
    auto &main_stream = render_device.lc_main_stream();
    auto &cmdlist = render_device.lc_main_cmd_list();
    render_plugin->on_rendering({}, pipe_ctx);
    // TODO: pipeline update
    //////////////// Test
    managed_device->end_managing(cmdlist);
    sm.on_frame_end(
        cmdlist,
        main_stream, managed_device);
    if (timeline_event && signal_fence > 0) [[likely]] {
        main_stream << timeline_event->signal(signal_fence);
    }
    render_device.render_loop_mtx().unlock();
}