#include "world_scene.h"
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/renderer.h>
#include <rbc_world_v2/light.h>
#include <rbc_world_v2/resource_loader.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_app/graphics_utils.h>
#include <rbc_core/binary_file_writer.h>

namespace rbc {
void WorldScene::_init_scene(GraphicsUtils *utils) {
    mesh = world::create_object<world::Mesh>();
    mesh->decode("cornell_box.obj");
    mesh->init_device_resource();
    utils->update_mesh_data(mesh->device_mesh().get(), false);// update through render-thread
    _mats.resize(mesh->submesh_count());
    LUISA_ASSERT(mesh->submesh_count() == 8);
    auto mat0 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]})"sv;
    auto mat1 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]})"sv;
    auto mat2 = R"({"type": "pbr", "specular_roughness": 0.2, "weight_metallic": 1.0, "base_albedo": [0.630, 0.065, 0.050]})"sv;
    if (luisa::filesystem::exists("logo.png")) {
        tex = world::create_object<world::Texture>();
        tex->decode("logo.png");
        tex->init_device_resource();
        utils->update_texture(tex->get_image());// update through render-thread
    }
    auto light_mat_desc = R"({"type": "pbr", "emission_luminance": [34, 24, 10], "base_albedo": [0, 0, 0]})"sv;

    auto basic_mat = RC<world::Material>{world::create_object<world::Material>()};
    auto left_wall_mat = RC<world::Material>{world::create_object<world::Material>()};
    auto right_wall_mat = RC<world::Material>{world::create_object<world::Material>()};
    auto light_mat = RC<world::Material>{world::create_object<world::Material>()};
    basic_mat->load_from_json(mat0);
    left_wall_mat->load_from_json(mat1);
    right_wall_mat->load_from_json(mat2);
    light_mat->load_from_json(light_mat_desc);
    _mats[0] = basic_mat;
    _mats[1] = basic_mat;
    _mats[2] = basic_mat;
    _mats[3] = std::move(left_wall_mat);
    _mats[4] = std::move(right_wall_mat);
    _mats[5] = basic_mat;
    _mats[6] = basic_mat;
    _mats[7] = std::move(light_mat);
    for (auto &i : _mats) {
        i->init_device_resource();
    }
    {
        auto entity = _entities.emplace_back(world::create_object<world::Entity>());
        auto transform = entity->add_component<world::Transform>();
        transform->set_pos(double3(0, -1, 2), true);
        transform->set_rotation(quaternion(
                                    make_float3x3(
                                        -1, 0, 0,
                                        0, 1, 0,
                                        0, 0, -1)),
                                true);
        auto render = entity->add_component<world::Renderer>();
        render->update_object(_mats, mesh);
    }
}
WorldScene::WorldScene(GraphicsUtils *utils) {
    auto &render_device = RenderDevice::instance();
    auto runtime_dir = render_device.lc_ctx().runtime_directory();
    luisa::filesystem::path meta_dir{"test_scene"};
    auto scene_root_dir = runtime_dir / meta_dir;
    auto entities_path = scene_root_dir / "scene.rbcmt";
    // write a demo scene
    if (!luisa::filesystem::exists(scene_root_dir) || luisa::filesystem::is_empty(scene_root_dir)) {
        luisa::filesystem::create_directories(scene_root_dir);
        world::init_resource_loader(runtime_dir, meta_dir);
        _init_scene(utils);
        // save scene
        vstd::HashMap<vstd::Guid> saved;
        auto write_file = [&](world::Resource *res) {
            if (!saved.try_emplace(res->guid()).second) return;
            res->set_path(
                scene_root_dir / (res->guid().to_string() + ".rbcb"),
                0);
            res->save_to_path();
            JsonSerializer js;
            res->serialize(world::ObjSerialize{
                js});
            auto blob = js.write_to();
            LUISA_ASSERT(!blob.empty());
            BinaryFileWriter file_writer(luisa::to_string(scene_root_dir / (res->guid().to_string() + ".rbcmt")));
            file_writer.write(blob);
        };
        write_file(mesh);
        for (auto &i : _mats) {
            write_file(i.get());
        }
        // write_scene
        JsonSerializer scene_ser{true};
        world::ObjSerialize ser{scene_ser};
        for (auto &i : _entities) {
            scene_ser.start_object();
            i->serialize(ser);
            scene_ser.add_last_scope_to_object();
        }
        BinaryFileWriter file_writer(luisa::to_string(entities_path));
        file_writer.write(scene_ser.write_to());
        return;
    }
    // load demo scene
    world::init_resource_loader(runtime_dir, meta_dir); // open project folder
    luisa::vector<std::byte> data;
    {
        BinaryFileStream file_stream(luisa::to_string(entities_path));
        LUISA_ASSERT(file_stream.valid());
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
    }
    // load test_scene/scene.rbcmt
    JsonDeSerializer entitie_deser(luisa::string_view((char const *)data.data(), data.size()));
    LUISA_ASSERT(entitie_deser.valid());
    uint64_t size = entitie_deser.last_array_size();
    _entities.reserve(size);
    for (auto i : vstd::range(size)) {
        auto e = _entities.emplace_back(world::create_object<world::Entity>());
        entitie_deser.start_object();
        e->deserialize(world::ObjDeSerialize{entitie_deser});
        entitie_deser.end_scope();
    }
    for (auto &i : _entities) {
        auto render = i->get_component<world::Renderer>();
        if (render) render->update_object();
    }
}
WorldScene::~WorldScene() {
    for (auto &i : _entities) {
        i->dispose();
    }
}
}// namespace rbc