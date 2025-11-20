#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>

int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;

    RenderDevice render_device;
    luisa::string backend = "dx";
    if (argc >= 2) {
        backend = argv[1];
    }
    // Create
    render_device.init(
        argv[0],
        backend);
    auto shader_path = render_device.lc_ctx().runtime_directory().parent_path() / "shader_build";

    vstd::optional<SceneManager> sm;
    sm.create(
        render_device.lc_ctx(),
        render_device.lc_device(),
        shader_path);
    SceneManager::set_instance(sm.ptr());

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

    // Destroy
    // sm.destroy();
    render_device.lc_main_stream().synchronize();
    render_device.shutdown();
}