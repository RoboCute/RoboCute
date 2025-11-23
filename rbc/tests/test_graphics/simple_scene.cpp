#include "simple_scene.h"
#include <rbc_graphics/mesh_builder.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
}// namespace rbc
void SimpleScene::_init_mesh() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &render_device = RenderDevice::instance();
    // create cube mesh
    MeshBuilder mesh_builder;
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
    RC<DeviceMesh> cube_mesh{new DeviceMesh{}};
    luisa::vector<std::byte> mesh_data;
    vstd::vector<uint> submesh_triangle_offset;// not used
    mesh_builder.write_to(mesh_data, submesh_triangle_offset);
    BinaryBlob blob{
        mesh_data.data(),
        mesh_data.size_bytes(),
        [mesh_data = std::move(mesh_data)](void *ptr) {}};
    cube_mesh->async_load_from_memory(std::move(blob), mesh_builder.vertex_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent(), mesh_builder.uv_count(), std::move(submesh_triangle_offset), false /*No need to build BLAS for origin mesh*/, true);
    device_meshes.emplace_back(std::move(cube_mesh));
    LUISA_INFO("Cube mesh loaded");
}
void SimpleScene::_init_material() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();
    material::OpenPBR mat{};
    for (auto &i : mat.base.albedo) {
        i = 0.5f;
    }
    mat.specular.roughness = 0.5f;
    // Make material instance
    default_mat_code = sm.mat_manager().emplace_mat_instance(
        mat,
        cmdlist,
        sm.bindless_allocator(),
        sm.buffer_uploader(),
        sm.dispose_queue(), material::PolymorphicMaterial::index<material::OpenPBR>);
}
void SimpleScene::tick() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();
    auto &cube_mesh = device_meshes[0];
    if (!tlas_loaded && cube_mesh->load_finished()) {
        tlas_loaded = true;
        float4x4 cube_transform = translation(float3(-1, -1, 3)) * scaling(float3(1, 1, 1));
        // add object
        tlas_indices.emplace_back(
            sm.accel_manager().emplace_mesh_instance(
                cmdlist, sm.host_upload_buffer(),
                sm.buffer_allocator(),
                sm.buffer_uploader(),
                sm.dispose_queue(),
                cube_mesh->mesh_data(),
                {&default_mat_code, 1},
                cube_transform));
        // add light
        float4x4 area_light_transform =
            translation(float3(1, 0.5f, 3)) *
            rotation(float3(1.0f, 0.0f, 0.0f), pi * 0.5f);
        lights.add_area_light(cmdlist, area_light_transform, float3(7, 6, 3) * 10.f);
    }
}
SimpleScene::SimpleScene() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    sm.mat_manager().emplace_mat_type<material::OpenPBR>(sm.bindless_allocator(), 65536, material::PolymorphicMaterial::index<material::OpenPBR>);
    sm.mat_manager().emplace_mat_type<material::Unlit>(sm.bindless_allocator(), 65536, material::PolymorphicMaterial::index<material::Unlit>);
    _init_mesh();
    _init_material();
}
SimpleScene::~SimpleScene() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    for (auto &i : tlas_indices) {
        sm.accel_manager().remove_mesh_instance(sm.buffer_allocator(), sm.buffer_uploader(), i);
    }
    sm.mat_manager().discard_mat_instance(default_mat_code);
}