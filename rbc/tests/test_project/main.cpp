#include <rbc_project/project.h>
#include <rbc_project/project_plugin.h>
#include <luisa/core/fiber.h>
#include <rbc_plugin/plugin.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/runtime_static.h>
#include <rbc_world/base_object.h>
#include <rbc_world/entity.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_core/atomic.h>
#include <rbc_world/resources/scene.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/atmosphere_component.h>
#include <rbc_world/importers/texture_loader.h>
#include <rbc_graphics/render_device.h>
int main(int argc, char *argv[]) {
    if (argc < 3) {
        LUISA_WARNING("Bad args, must be #backend# #scene path#");
        return 1;
    }
    using namespace luisa;
    using namespace rbc;
    constexpr bool gpu_less_mode = false;
    luisa::fiber::scheduler scheduler;
    luisa::unique_ptr<GraphicsUtils> utils;
    if (gpu_less_mode) {
        LUISA_WARNING("GPU-less mode enabled, some function may cause error.");
    } else {
        utils = luisa::make_unique<GraphicsUtils>();
    }
    auto dispose_runtime_static = vstd::scope_exit([&] {
        if (gpu_less_mode) {
            world::destroy_world();
        } else {
            utils->dispose([&]() {
                world::destroy_world();
            });
        }
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
    });
    RuntimeStaticBase::init_all();
    PluginManager::init();
    luisa::string backend = argv[1];
    if (!gpu_less_mode) {
        utils->init_device(
            argv[0],
            backend.c_str());
        utils->init_graphics(RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils->backend_name()));
    }
    auto binary_dir = "test_project_meta";
    if (!luisa::filesystem::is_directory(binary_dir)) {
        luisa::filesystem::create_directories(binary_dir);
    }
    world::init_world(binary_dir);
    // TODO: test project
    auto project_plugin_module = PluginManager::instance().load_module("rbc_project_plugin");
    auto project_plugin = project_plugin_module->invoke<ProjectPlugin *()>(
        "get_project_plugin");
    auto proj = luisa::unique_ptr<IProject>(project_plugin->create_project(argv[2]));
    proj->scan_project();
    auto bunny_obj = proj->import_assets("bunny.obj", TypeInfo::get<world::MeshResource>().md5());
    LUISA_INFO("Importing bunny.obj.");
    LUISA_ASSERT(bunny_obj);
    LUISA_INFO("Importing cornell_box.obj.");
    auto cbox_mesh = proj->import_assets("cornell_box.obj", TypeInfo::get<world::MeshResource>().md5()).cast_static<world::MeshResource>();
    LUISA_ASSERT(cbox_mesh);
    LUISA_INFO("Importing sky.exr.");
    auto sky_tex = proj->import_assets("sky.exr", TypeInfo::get<world::TextureResource>().md5());
    LUISA_ASSERT(sky_tex);
    LUISA_INFO("Importing test_grid.png.");
    LUISA_ASSERT(proj->import_assets("test_grid.png", TypeInfo::get<world::TextureResource>().md5()));
    utils->tex_loader()->finish_task();
    // load scene
    auto scene = proj->import_assets("test_scene.scene", TypeInfo::get<world::SceneResource>().md5());
    if (!scene)// create_scene
    {
        LUISA_INFO("Scene not found, start init scene.");
        auto s = world::create_object<world::SceneResource>();
        scene = s;
        // add cbox
        {
            cbox_mesh->wait_loading();
            luisa::vector<RC<world::MaterialResource>> mats;
            mats.resize(cbox_mesh->submesh_count());
            LUISA_ASSERT(cbox_mesh->submesh_count() == 8);
            auto mat0 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]})"sv;
            auto mat1 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]})"sv;
            auto mat2 = R"({"type": "pbr", "specular_roughness": 0.2, "weight_metallic": 1.0, "base_albedo": [0.630, 0.065, 0.050]})"sv;
            auto light_mat_desc = R"({"type": "pbr", "emission_luminance": [34, 24, 10], "base_albedo": [0, 0, 0]})"sv;

            auto basic_mat = RC<world::MaterialResource>{world::create_object<world::MaterialResource>()};
            auto left_wall_mat = RC<world::MaterialResource>{world::create_object<world::MaterialResource>()};
            auto right_wall_mat = RC<world::MaterialResource>{world::create_object<world::MaterialResource>()};
            auto light_mat = RC<world::MaterialResource>{world::create_object<world::MaterialResource>()};
            basic_mat->load_from_json(mat0);
            left_wall_mat->load_from_json(mat1);
            right_wall_mat->load_from_json(mat2);
            light_mat->load_from_json(light_mat_desc);
            basic_mat->save_to_path();
            light_mat->save_to_path();
            left_wall_mat->save_to_path();
            right_wall_mat->save_to_path();

            mats[0] = basic_mat;
            mats[1] = basic_mat;
            mats[2] = basic_mat;
            mats[3] = std::move(left_wall_mat);
            mats[4] = std::move(right_wall_mat);
            mats[5] = basic_mat;
            mats[6] = basic_mat;
            mats[7] = light_mat;
            // material not loaded from assets, but generated

            auto entity = s->get_or_add_entity(vstd::Guid{true});
            auto atmo_component = entity->add_component<world::AtmosphereComponent>();
            atmo_component->hdri = std::move(sky_tex);
            auto transform = entity->add_component<world::TransformComponent>();
            transform->set_pos(double3(0, -1, 2), true);
            auto rot = quaternion(
                make_float3x3(
                    -1, 0, 0,
                    0, 1, 0,
                    0, 0, -1));
            transform->set_rotation(rot, true);
            auto render = entity->add_component<world::RenderComponent>();
            render->update_object(mats, cbox_mesh.get());
        }
        // add bunny
        {
            bunny_obj->wait_loading();
            auto entity = s->get_or_add_entity(vstd::Guid{true});
            auto transform = entity->add_component<world::TransformComponent>();
            transform->set_pos(double3(0, 3, 2), false);
            transform->set_scale(double3(10), false);
            auto render = entity->add_component<world::RenderComponent>();
            render->update_object({}, static_cast<world::MeshResource *>(bunny_obj.get()));
        }
        scene->save_to_path();
        // copy from binary to assets: TODO: do we have elegant way?
        luisa::filesystem::copy(
            scene->path(),
            proj->root_path() / "test_scene.scene");
        IProject::FileMeta file_meta{
            .guid = scene->guid(),
            .meta_info = "{}",
            .type_id = scene->type_id()};
        proj->unsafe_write_file_meta(
            "test_scene.scene",
            {&file_meta, 1});
    }
    return 0;
}