#include "world_scene.h"
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/renderer.h>
#include <rbc_world_v2/light.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_app/graphics_utils.h>

namespace rbc {
WorldScene::WorldScene(GraphicsUtils *utils) {
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
WorldScene::~WorldScene() {
    for (auto &i : _entities) {
        i->dispose();
    }
}
}// namespace rbc