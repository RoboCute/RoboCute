#include "simple_scene.h"
#include <rbc_graphics/mesh_builder.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
#include <rbc_graphics/materials.h>
#include <material/mats.inl>
}// namespace rbc
static const luisa::float3 light_emission{luisa::float3(7, 6, 3) * 1000.f};
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
void SimpleScene::_init_tlas() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();
    auto &cube_mesh = device_meshes[0];
    cube_mesh->wait_finished();
    cube_pos = float3(-1, -1, 3);
    float4x4 cube_transform = translation(cube_pos);
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
    light_pos = float3(0.5, 0.5f, 1);
    float4x4 area_light_transform =
        translation(light_pos) *
        rotation(float3(1.0f, 0.0f, 0.0f), pi * 0.5f) * scaling(0.1f);
    // light_id.emplace_back(lights.add_area_light(cmdlist, area_light_transform, light_emission));
    auto &quad_mesh = device_meshes.emplace_back(new DeviceMesh{});
    uint quad_data[4 * 4 + 4 * 2 + 6];
    auto quad_verts = (luisa::float3 *)quad_data;
    quad_verts[0] = float3(-0.5, -0.5, 0);
    quad_verts[1] = float3(-0.5, 0.5, 0);
    quad_verts[2] = float3(0.5, -0.5, 0);
    quad_verts[3] = float3(0.5, 0.5, 0);
    auto quad_uvs = (luisa::float2 *)(quad_verts + 4);
    quad_uvs[0] = float2(0, 0);
    quad_uvs[1] = float2(0, 1);
    quad_uvs[2] = float2(1, 0);
    quad_uvs[3] = float2(1, 1);
    auto quad_tri = (geometry::Triangle *)(quad_uvs + 4);
    quad_tri[0] = geometry::Triangle{0, 2, 1};
    quad_tri[1] = geometry::Triangle{1, 2, 3};
    quad_mesh->async_load_from_memory(
        luisa::BinaryBlob{
            (std::byte *)quad_data,
            vstd::array_byte_size(quad_data),
            {}},
        4,
        false,
        false,
        1,
        {},
        true,
        false,
        true);
    sm.buffer_uploader().commit(cmdlist, sm.host_upload_buffer());
    light_id.emplace_back(lights.add_mesh_light_sync(
        cmdlist,
        quad_mesh,
        area_light_transform,
        {&light_mat_code, 1}));
}
void SimpleScene::_init_material() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = render_device.lc_main_cmd_list();
    material::OpenPBR mat{};
    mat.base.albedo = make_half3((half)0.5f, (half)0.5f, (half)0.5f);
    // mat.weight.metallic = 1.0f;
    mat.specular.specular_color_and_rough.w = 0.5f;
    mat.specular.roughness_anisotropy_angle = 0.7f;
    // Make material instance
    default_mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial>(
        mat,
        cmdlist,
        sm.bindless_allocator(),
        sm.buffer_uploader(),
        sm.dispose_queue());
    material::Unlit light_mat{};
    for (auto i : vstd::range(3))
        light_mat.color[i] = light_emission[i];
    light_mat_code = sm.mat_manager().emplace_mat_instance<material::PolymorphicMaterial>(
        light_mat,
        cmdlist,
        sm.bindless_allocator(),
        sm.buffer_uploader(),
        sm.dispose_queue());
}
SimpleScene::SimpleScene(rbc::Lights &lights) : lights(lights) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    sm.mat_manager().emplace_mat_type<material::OpenPBR>(sm.bindless_allocator(), 65536, material::PolymorphicMaterial::index<material::OpenPBR>);
    sm.mat_manager().emplace_mat_type<material::Unlit>(sm.bindless_allocator(), 65536, material::PolymorphicMaterial::index<material::Unlit>);
    _init_mesh();
    _init_material();
    _init_tlas();
}
SimpleScene::~SimpleScene() {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    for (auto &i : tlas_indices) {
        sm.accel_manager().remove_mesh_instance(sm.buffer_allocator(), sm.buffer_uploader(), i);
    }
    for (auto &i : light_id) {
        lights.remove_mesh_light(i);
    }
    sm.mat_manager().discard_mat_instance(default_mat_code);
    sm.mat_manager().discard_mat_instance(light_mat_code);
}
void SimpleScene::move_cube(luisa::float3 pos) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    cube_pos += pos;
    sm.accel_manager().set_mesh_instance(
        render_device.lc_main_cmd_list(),
        sm.buffer_uploader(),
        tlas_indices[0],
        translation(cube_pos), ~0ull, true);
}
void SimpleScene::move_light(luisa::float3 pos) {
    using namespace rbc;
    using namespace luisa;
    using namespace luisa::compute;
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    light_pos += pos;
    // lights.update_area_light(
    //     render_device.lc_main_cmd_list(),
    //     light_id[0],
    //     translation(light_pos) *
    //         rotation(float3(1.0f, 0.0f, 0.0f), pi * 0.5f) * scaling(0.1f),
    //     light_emission);
    lights.update_mesh_light_sync(
        render_device.lc_main_cmd_list(),
        light_id[0],
        translation(light_pos) *
            rotation(float3(1.0f, 0.0f, 0.0f), pi * 0.5f) * scaling(0.1f),
        {&light_mat_code, 1});
}