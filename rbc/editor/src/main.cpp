#include <QApplication>
#include <QFile>
#include "RBCEditor/MainWindow.h"
#include "RBCEditor/EditorEngine.h"

#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_runtime/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>

#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>

#include "RBCEditor/runtime/RenderUtils.h"
#include "RBCEditor/runtime/RenderScene.h"

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

int main(int argc, char **argv) {

    int ret = 0;
    log_level_info();

    if (true) {
        QApplication app(argc, argv);
        QFile f(":/main.qss");
        QString styleSheet = "";
        if (f.open(QIODevice::ReadOnly)) {
            styleSheet = f.readAll();
        }
        app.setStyleSheet(styleSheet);
        rbc::EditorEngine::instance().init(argc, argv);
        {
            MainWindow window;
            window.setupUi();
            window.startSceneSync("http://127.0.0.1:5555");
            window.show();
            ret = QApplication::exec();
        }
        rbc::EditorEngine::instance().shutdown();
    }

    if (false) {
        luisa::fiber::scheduler scheduler;
        luisa::string backend = "dx";
        if (argc >= 2) {
            backend = argv[1];
        }
        GraphicsUtils utils;
        utils.init_device(
            argv[0],
            backend.c_str());
        utils.init_graphics(
            RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name));
        utils.init_render();
        utils.render_plugin->update_skybox("../sky.bytes", uint2(4096, 2048));
        utils.init_display("test_graphics", uint2(1024), true, true);

        uint64_t frame_index = 0;
        // Present is ping-pong frame-buffer and compute is triple-buffer
        Clock clk;
        double last_frame_time = 0;
        vstd::optional<rbc::SimpleScene> simple_scene;
        simple_scene.create(*utils.lights);
        // Test FOV
        vstd::optional<float3> cube_move, light_move;
        bool reset = false;

        utils.window->set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
            if (action != Action::ACTION_PRESSED) return;
            frame_index = 0;
            reset = true;
            switch (key) {
                case Key::KEY_SPACE: {
                    LUISA_INFO("Reset frame");
                } break;
                case Key::KEY_W: {
                    light_move.create();
                    *light_move += float3(0, 0.1, 0);
                } break;
                case Key::KEY_S: {
                    light_move.create();
                    *light_move += float3(0, -0.1, 0);
                } break;
                case Key::KEY_A: {
                    light_move.create();
                    *light_move += float3(-0.1, 0, 0);
                } break;
                case Key::KEY_D: {
                    light_move.create();
                    *light_move += float3(0.1, 0, 0);
                } break;
                case Key::KEY_Q: {
                    light_move.create();
                    *light_move += float3(0, 0, -0.1);
                } break;
                case Key::KEY_E: {
                    light_move.create();
                    *light_move += float3(0, 0, 0.1);
                } break;
                case Key::KEY_UP: {
                    cube_move.create();
                    *cube_move += float3(0, 0.1, 0);
                } break;
                case Key::KEY_DOWN: {
                    cube_move.create();
                    *cube_move += float3(0, -0.1, 0);
                } break;
                case Key::KEY_LEFT: {
                    cube_move.create();
                    *cube_move += float3(-0.1, 0, 0);
                } break;
                case Key::KEY_RIGHT: {
                    cube_move.create();
                    *cube_move += float3(0.1, 0, 0);
                } break;
            }
        });
        uint2 window_size = utils.window->size();
        utils.window->set_window_size_callback([&](uint2 size) {
            window_size = size;
        });

        auto &cam = utils.render_plugin->get_camera(utils.display_pipe_ctx);
        cam.fov = radians(80.f);
        while (!utils.should_close()) {
            if (reset) {
                reset = false;
                utils.reset_frame();
            }
            utils.tick([&]() {
                if (any(window_size != utils.dst_image.size())) {
                    utils.resize_swapchain(window_size);
                }
                cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
                auto &frame_settings = utils.render_settings.read_mut<rbc::FrameSettings>();
                auto &dst_img = utils.dst_image;
                frame_settings.render_resolution = dst_img.size();
                frame_settings.display_resolution = dst_img.size();
                frame_settings.dst_img = &dst_img;
                auto time = clk.toc();
                auto delta_time = time - last_frame_time;
                last_frame_time = time;
                frame_settings.delta_time = (float)delta_time;
                frame_settings.time = time;
                frame_settings.frame_index = frame_index;
                ++frame_index;
                // scene logic
                if (cube_move) {
                    simple_scene->move_cube(*cube_move);
                    cube_move.destroy();
                }
                if (light_move) {
                    simple_scene->move_light(*light_move);
                    light_move.destroy();
                }
            });
        }
        // rpc_hook.shutdown_remote();
        utils.dispose([&]() {
            auto pipe_settings_json = utils.render_settings.serialize_to_json();
            if (pipe_settings_json.data()) {
                LUISA_INFO("{}", luisa::string_view{
                                     (char const *)pipe_settings_json.data(),
                                     pipe_settings_json.size()});
            }
            // destroy render-pipeline
            simple_scene.destroy();
        });
    }

    return ret;
}
