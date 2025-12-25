#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_app/graphics_utils.h>
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_app/camera_controller.h>
#include <rbc_core/runtime_static.h>
#include <rbc_plugin/plugin_manager.h>
#include <tracy_wrapper.h>
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/texture_loader.h>
#include <rbc_world/importers/gltf_scene_loader.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/base_object.h>
#include <luisa/core/logging.h>

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

int main(int argc, char *argv[]) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;

    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    auto dispose_runtime_static = vstd::scope_exit([] {
        RuntimeStaticBase::dispose_all();
    });

    luisa::string backend = "dx";
    if (argc >= 2) {
        backend = argv[1];
    }

    GraphicsUtils utils;
    PluginManager::init();
    utils.init_device(
        argv[0],
        backend.c_str());
    utils.init_graphics(
        RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name()));
    utils.init_render();
    utils.init_display_with_window(luisa::string{"model_viewer_"} + utils.backend_name(), uint2(1024), true);

    uint64_t frame_index = 0;
    Clock clk;
    double last_frame_time = 0;

    // Initialize world resource loader
    auto &render_device = RenderDevice::instance();
    auto runtime_dir = render_device.lc_ctx().runtime_directory();
    luisa::filesystem::path resource_dir = runtime_dir / "model_viewer_resources";
    if (!luisa::filesystem::exists(resource_dir)) {
        luisa::filesystem::create_directories(resource_dir);
    }

    world::init_world(resource_dir);

    // Load skybox
    RC<world::TextureResource> skybox;
    {
        RBCZoneScopedN("Load Skybox");
        world::TextureLoader tex_loader;
        // Try to load sky.exr from runtime directory or use a default path
        luisa::filesystem::path sky_path = runtime_dir / "sky.exr";
        if (!luisa::filesystem::exists(sky_path)) {
            // Try test_scene directory
            sky_path = runtime_dir / "test_scene" / "sky.exr";
        }
        if (luisa::filesystem::exists(sky_path)) {
            skybox = tex_loader.decode_texture(sky_path, 1, false);
            if (skybox) {
                tex_loader.finish_task();
                skybox->init_device_resource();
                utils.update_texture(skybox->get_image());
                RC<DeviceImage> image{skybox->get_image()};
                utils.render_plugin()->update_skybox(image);
                LUISA_INFO("Skybox loaded from: {}", luisa::to_string(sky_path));
            }
        } else {
            LUISA_WARNING("Skybox file not found at: {}, rendering without skybox", luisa::to_string(sky_path));
        }
    }

    // Load GLTF model using runtime loader
    // luisa::filesystem::path gltf_path = "d:/ws/data/assets/models/Cube/Cube.gltf";
    luisa::filesystem::path gltf_path = "d:/ws/data/assets/anim_test/test_anim.gltf";
    // luisa::filesystem::path gltf_path = "d:/ws/data/assets/models/sponza/scene.gltf";
    if (argc >= 3) {
        gltf_path = argv[2];
    }

    RC<world::Entity> entity;
    RC<world::MeshResource> loaded_mesh;
    luisa::vector<RC<world::MaterialResource>> loaded_materials;
    {
        RBCZoneScopedN("Load GLTF Scene");
        auto scene_data = world::GltfSceneLoader::load_scene(gltf_path);

        if (!scene_data.mesh || scene_data.mesh->empty()) {
            LUISA_ERROR("Failed to load GLTF model from: {}", luisa::to_string(gltf_path));
            return 1;
        }

        // Store resources for cleanup
        loaded_mesh = scene_data.mesh;
        loaded_materials = std::move(scene_data.materials);

        // Initialize mesh device resource
        loaded_mesh->init_device_resource();
        utils.update_mesh_data(loaded_mesh->device_mesh().get(), false);

        // Initialize texture device resources
        for (auto &tex : scene_data.textures) {
            if (tex) {
                utils.update_texture(tex->get_image());
            }
        }

        // Ensure we have enough materials for all submeshes
        size_t submesh_count = loaded_mesh->submesh_count();
        while (loaded_materials.size() < submesh_count) {
            // Use the first material or create a default one
            if (!loaded_materials.empty()) {
                loaded_materials.push_back(loaded_materials[0]);
            } else {
                auto default_mat = RC<world::MaterialResource>(world::create_object<world::MaterialResource>());
                default_mat->load_from_json(R"({"type": "pbr", "base_albedo": [0.8, 0.8, 0.8]})");
                loaded_materials.push_back(std::move(default_mat));
            }
        }

        // Initialize materials
        for (auto &mat : loaded_materials) {
            if (mat) {
                mat->init_device_resource();
            }
        }

        // Create entity with the loaded mesh
        entity = world::create_object<world::Entity>();
        auto transform = entity->add_component<world::TransformComponent>();
        transform->set_pos(double3(0, 0, 0), true);

        auto render = entity->add_component<world::RenderComponent>();
        render->start_update_object(loaded_materials, loaded_mesh.get());
    }

    // Camera setup
    auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
    CameraController cam_controller;
    cam_controller.camera = &cam;
    cam.fov = radians(80.f);
    cam.position = double3(0, 0, 5);

    CameraController::Input camera_input;
    uint2 window_size = utils.window()->size();

    utils.window()->set_mouse_callback([&](MouseButton button, Action action, float2 xy) {
        if (button == MOUSE_BUTTON_2) {
            if (action == Action::ACTION_PRESSED) {
                camera_input.is_mouse_right_down = true;
            } else if (action == Action::ACTION_RELEASED) {
                camera_input.is_mouse_right_down = false;
            }
        }
    });

    utils.window()->set_cursor_position_callback([&](float2 xy) {
        camera_input.mouse_cursor_pos = xy;
    });

    utils.window()->set_key_callback([&](Key key, KeyModifiers modifiers, Action action) {
        bool pressed = (action == Action::ACTION_PRESSED);
        switch (key) {
            case Key::KEY_SPACE:
                camera_input.is_space_down = pressed;
                break;
            case Key::KEY_RIGHT_SHIFT:
            case Key::KEY_LEFT_SHIFT:
                camera_input.is_shift_down = pressed;
                break;
            case Key::KEY_W:
                camera_input.is_front_dir_key_pressed = pressed;
                break;
            case Key::KEY_S:
                camera_input.is_back_dir_key_pressed = pressed;
                break;
            case Key::KEY_A:
                camera_input.is_left_dir_key_pressed = pressed;
                break;
            case Key::KEY_D:
                camera_input.is_right_dir_key_pressed = pressed;
                break;
            case Key::KEY_Q:
                camera_input.is_up_dir_key_pressed = pressed;
                break;
            case Key::KEY_E:
                camera_input.is_down_dir_key_pressed = pressed;
                break;
        }
    });

    utils.window()->set_window_size_callback([&](uint2 size) {
        window_size = size;
    });

    while (!utils.should_close()) {
        RBCFrameMark;

        {
            RBCZoneScopedN("Main Loop");

            {
                RBCZoneScopedN("Poll Events");
                if (utils.window())
                    utils.window()->poll_events();
            }

            auto &cam = utils.render_plugin()->get_camera(utils.default_pipe_ctx());
            if (any(window_size != utils.dst_image().size())) {
                RBCZoneScopedN("Resize Swapchain");
                utils.resize_swapchain(window_size);
                frame_index = 0;
            }

            float delta_time = 0.0f;
            {
                RBCZoneScopedN("Update Camera");
                camera_input.viewport_size = make_float2(window_size);
                cam.aspect_ratio = (float)window_size.x / (float)window_size.y;
                auto time = clk.toc();
                delta_time = (time - last_frame_time) * 1e-3f;
                RBCPlot("Frame Time (ms)", delta_time * 1000.0f);
                cam_controller.grab_input_from_viewport(camera_input, delta_time);
                if (cam_controller.any_changed())
                    frame_index = 0;
                last_frame_time = time;
            }

            {
                RBCZoneScopedN("Render Tick");
                auto tick_stage = GraphicsUtils::TickStage::PathTracingPreview;
                utils.tick(
                    static_cast<float>(delta_time),
                    frame_index,
                    window_size,
                    tick_stage);
            }

            ++frame_index;
            RBCPlot("Frame Index", static_cast<float>(frame_index));
        }
    }

    utils.dispose([&]() {
        // remove ref-counted resources
        loaded_materials.clear();
        loaded_mesh.reset();
        skybox.reset();
        // Dispose entity first
        if (entity) {
            entity.reset();
        }
        // Destroy world (this will check for leaks)
        world::destroy_world();
    });

    return 0;
}
