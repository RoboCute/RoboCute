#include "world_scene.h"
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/renderer.h>
#include <rbc_world_v2/light.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_app/graphics_utils.h>

namespace rbc {
WorldScene::WorldScene(GraphicsUtils *utils) {
    MeshBuilder mesh_builder;
    auto emplace = [&] {
        mesh_builder.position.push_back(float3(-0.5f, -0.5f, -0.5f));// 0: Left Bottom Back
        mesh_builder.position.push_back(float3(-0.5f, -0.5f, 0.5f)); // 1: Left Bottom Front
        mesh_builder.position.push_back(float3(0.5f, -0.5f, -0.5f)); // 2: Right Buttom Back
        mesh_builder.position.push_back(float3(0.5f, -0.5f, 0.5f));  // 3: Right Buttom Front
        mesh_builder.position.push_back(float3(-0.5f, 0.5f, -0.5f)); // 4: Left Up Back
        mesh_builder.position.push_back(float3(-0.5f, 0.5f, 0.5f));  // 5: Left Up Front
        mesh_builder.position.push_back(float3(0.5f, 0.5f, -0.5f));  // 6: Right Up Back
        mesh_builder.position.push_back(float3(0.5f, 0.5f, 0.5f));   // 7: Right Up Front
        auto &triangle = mesh_builder.triangle_indices.emplace_back();
        // Buttom face
        triangle.emplace_back(0);
        triangle.emplace_back(1);
        triangle.emplace_back(2);
        triangle.emplace_back(1);
        triangle.emplace_back(3);
        triangle.emplace_back(2);
        // Up face
        triangle.emplace_back(4);
        triangle.emplace_back(5);
        triangle.emplace_back(6);
        triangle.emplace_back(5);
        triangle.emplace_back(7);
        triangle.emplace_back(6);
        // Left face
        triangle.emplace_back(0);
        triangle.emplace_back(1);
        triangle.emplace_back(4);
        triangle.emplace_back(1);
        triangle.emplace_back(5);
        triangle.emplace_back(4);
        // Right face
        triangle.emplace_back(2);
        triangle.emplace_back(3);
        triangle.emplace_back(6);
        triangle.emplace_back(3);
        triangle.emplace_back(7);
        triangle.emplace_back(6);
        // Back face
        triangle.emplace_back(0);
        triangle.emplace_back(2);
        triangle.emplace_back(4);
        triangle.emplace_back(2);
        triangle.emplace_back(6);
        triangle.emplace_back(4);
        // Front face
        triangle.emplace_back(1);
        triangle.emplace_back(3);
        triangle.emplace_back(5);
        triangle.emplace_back(3);
        triangle.emplace_back(7);
        triangle.emplace_back(5);
    };
    emplace();
    auto vert_start = mesh_builder.position.size();
    emplace();
    for (auto &i : mesh_builder.triangle_indices.back()) {
        i += vert_start;
    }
    for (auto &i : luisa::span{mesh_builder.position}.subspan(vert_start)) {
        i += make_float3(0, 1.1f, 0);
    }

    luisa::vector<std::byte> mesh_data;
    vstd::vector<uint> submesh_triangle_offset;// not used
    mesh_builder.write_to(mesh_data, submesh_triangle_offset);
    mesh = world::create_object<world::Mesh>();
    mesh->create_empty(
        {}, std::move(submesh_triangle_offset),
        0, mesh_builder.vertex_count(), mesh_builder.indices_count() / 3,
        mesh_builder.uv_count(),
        mesh_builder.contained_normal(),
        mesh_builder.contained_tangent());
    *mesh->host_data() = std::move(mesh_data);
    utils->update_mesh_data(mesh->device_mesh().get(), false);
    mat0 = world::create_object<world::Material>();
    mat1 = world::create_object<world::Material>();
    mat0->load_from_json("{\"type\": \"pbr\"}"sv);
    mat1->load_from_json("{\"type\": \"pbr\", \"base_albedo\": [1, 0.8, 0.6]}"sv);
    mat0->prepare_material();
    mat1->prepare_material();
    // object
    {
        auto entity = _entities.emplace_back(world::create_object<world::Entity>());
        auto transform = entity->add_component<world::Transform>();
        auto render = entity->add_component<world::Renderer>();
        transform->set_pos(make_double3(0, 0, 3), true);
        RC<world::Material> mats[] = {
            RC<world::Material>(mat0),
            RC<world::Material>(mat1)};
        render->update_object(mats, mesh);
    }
    // light
    {
        auto entity = _entities.emplace_back(world::create_object<world::Entity>());
        auto transform = entity->add_component<world::Transform>();
        auto light = entity->add_component<world::Light>();
        transform->set_pos(make_double3(0.5, 0.5, 2), true);
        transform->set_scale(double3(0.1), true);
        light->add_area_light(float3(100, 100, 100), true);
    }
}
WorldScene::~WorldScene() {
    for (auto &i : _entities) {
        i->dispose();
    }
}
}// namespace rbc