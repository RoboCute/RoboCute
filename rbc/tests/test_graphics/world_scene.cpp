#include "world_scene.h"
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/render_component.h>
#include <rbc_world_v2/light.h>
#include <rbc_world_v2/resource_loader.h>
#include <rbc_world_v2/texture_loader.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_app/graphics_utils.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_render/click_manager.h>

namespace rbc {
void WorldScene::_init_scene(GraphicsUtils *utils) {
    cbox_mesh = world::create_object<world::Mesh>();
    cbox_mesh->decode("cornell_box.obj");
    cbox_mesh->init_device_resource();
    utils->update_mesh_data(cbox_mesh->device_mesh().get(), false);// update through render-thread
    _mats.resize(cbox_mesh->submesh_count());
    LUISA_ASSERT(cbox_mesh->submesh_count() == 8);
    auto mat0 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.725, 0.710, 0.680]})"sv;
    auto mat1 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]})"sv;
    auto mat2 = R"({"type": "pbr", "specular_roughness": 0.2, "weight_metallic": 1.0, "base_albedo": [0.630, 0.065, 0.050]})"sv;
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
    _mats[7] = light_mat;
    for (auto &i : _mats) {
        i->init_device_resource();
    }
    _mats[5] = nullptr;
    {
        auto entity = _entities.emplace_back(world::create_object<world::Entity>());
        auto transform = entity->add_component<world::Transform>();
        transform->set_pos(double3(0, -1, 2), true);
        auto rot = quaternion(
            make_float3x3(
                -1, 0, 0,
                0, 1, 0,
                0, 0, -1));
        transform->set_rotation(rot, true);
        auto render = entity->add_component<world::RenderComponent>();
        render->update_object(_mats, cbox_mesh);
    }
    MeshBuilder mesh_builder;
    mesh_builder.position.emplace_back(make_float3(-0.5f, -0.5f, 0));
    mesh_builder.position.emplace_back(make_float3(-0.5f, 0.5f, 0));
    mesh_builder.position.emplace_back(make_float3(0.5f, -0.5f, 0));
    mesh_builder.position.emplace_back(make_float3(0.5f, 0.5f, 0));
    auto &uv = mesh_builder.uvs.emplace_back();
    uv.emplace_back(make_float2(0, 0));
    uv.emplace_back(make_float2(0, 1));
    uv.emplace_back(make_float2(1, 0));
    uv.emplace_back(make_float2(1, 1));
    auto &tris = mesh_builder.triangle_indices.emplace_back();
    tris.emplace_back(0);
    tris.emplace_back(1);
    tris.emplace_back(2);
    tris.emplace_back(1);
    tris.emplace_back(3);
    tris.emplace_back(2);
    quad_mesh = world::create_object<world::Mesh>();
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> quad_bytes;
    mesh_builder.write_to(quad_bytes, submesh_offsets);
    quad_mesh->create_empty({}, std::move(submesh_offsets), 0, mesh_builder.vertex_count(), mesh_builder.indices_count() / 3, mesh_builder.uv_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent(), 0, 0);
    auto s = quad_bytes.size_bytes();
    *quad_mesh->host_data() = std::move(quad_bytes);
    quad_mesh->init_device_resource();
    utils->update_mesh_data(quad_mesh->device_mesh().get(), false);// update through render-thread

    world::TextureLoader tex_loader;
    tex = tex_loader.decode_texture(
        "test_grid.png",
        16,
        true);
    skybox = tex_loader.decode_texture(
        "sky.exr",
        1,
        false);
    // write guid
    auto sky_guid = skybox->guid();
    {
        BinaryFileWriter file_writer{"test_scene/sky_guid.txt"};
        auto sky_str = sky_guid.to_string();
        file_writer.write({(std::byte *)sky_str.data(),
                           sky_str.size()});
    }

    tex_loader.finish_task();
    // TODO: transform from regular tex to vt need reload device-image
    // tex->pack_to_tile();
    tex->init_device_resource();
    skybox->init_device_resource();
    utils->render_plugin()->update_skybox(skybox->get_image());
    utils->update_texture(skybox->get_image());// update through render-thread

    auto quad_entity = _entities.emplace_back(world::create_object<world::Entity>());
    auto quad_trans = quad_entity->add_component<world::Transform>();
    auto quad_render = quad_entity->add_component<world::RenderComponent>();
    quad_trans->set_pos(make_double3(0, 0, 4), true);
    quad_trans->set_scale(double3(10, 10, 10), true);
    auto quad_mat = world::create_object<world::Material>();
    auto quad_mat_str = luisa::format(R"({{"type": "pbr", "base_albedo_tex": "{}"}})", tex->guid().to_base64());
    quad_mat->load_from_json(quad_mat_str);
    quad_mat->init_device_resource();
    auto mat_start_idx = _mats.size();
    _mats.emplace_back(quad_mat);
    quad_render->update_object(
        luisa::span{_mats}.subspan(mat_start_idx, 1),
        quad_mesh);
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
            res->serialize_meta(world::ObjSerialize{
                js});
            auto blob = js.write_to();
            LUISA_ASSERT(!blob.empty());
            BinaryFileWriter file_writer(luisa::to_string(scene_root_dir / (res->guid().to_string() + ".rbcmt")));
            file_writer.write(blob);
        };
        write_file(cbox_mesh);
        write_file(quad_mesh);
        write_file(tex.get());
        write_file(skybox.get());
        for (auto &i : _mats) {
            if (i)
                write_file(i.get());
        }
        // write_scene
        JsonSerializer scene_ser{true};
        world::ObjSerialize ser{scene_ser};
        for (auto &i : _entities) {
            scene_ser.start_object();
            i->serialize_meta(ser);
            scene_ser.add_last_scope_to_object();
        }
        BinaryFileWriter file_writer(luisa::to_string(entities_path));
        file_writer.write(scene_ser.write_to());
    } else {
        // load demo scene
        world::init_resource_loader(runtime_dir, meta_dir);// open project folder

        // load skybox
        {
            BinaryFileStream file_stream{"test_scene/sky_guid.txt"};
            luisa::string sky_str;
            if (file_stream.length() != 32) {
                LUISA_ERROR("test_scene/sky_guid.txt is bad.");
            }
            sky_str.resize(32);
            file_stream.read({(std::byte *)sky_str.data(),
                              sky_str.size()});
            auto sky_guid = vstd::Guid::TryParseGuid(sky_str);
            LUISA_ASSERT(sky_guid);
            skybox = world::load_resource(*sky_guid, true);
        }
        skybox->wait_load_finished();
        utils->render_plugin()->update_skybox(skybox->get_image());
        
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
            e->deserialize_meta(world::ObjDeSerialize{entitie_deser});
            entitie_deser.end_scope();
        }
        for (auto &i : _entities) {
            auto render = i->get_component<world::RenderComponent>();
            if (render) render->update_object();
        }
    }
    _set_gizmos();
}
void WorldScene::_set_gizmos() {
    MeshBuilder mesh_builder;
    auto emplace = [&](float3 scale, float3 offset, float3 color) {
        auto idx = mesh_builder.position.size();
        mesh_builder.position.push_back(float3(0.0f, 0.0f, 0.0f));// 0: Left Bottom Back
        mesh_builder.position.push_back(float3(0.0f, 0.0f, 1.0f));// 1: Left Bottom Front
        mesh_builder.position.push_back(float3(1.0f, 0.0f, 0.0f));// 2: Right Buttom Back
        mesh_builder.position.push_back(float3(1.0f, 0.0f, 1.0f));// 3: Right Buttom Front
        mesh_builder.position.push_back(float3(0.0f, 1.0f, 0.0f));// 4: Left Up Back
        mesh_builder.position.push_back(float3(0.0f, 1.0f, 1.0f));// 5: Left Up Front
        mesh_builder.position.push_back(float3(1.0f, 1.0f, 0.0f));// 6: Right Up Back
        mesh_builder.position.push_back(float3(1.0f, 1.0f, 1.0f));// 7: Right Up Front
        for (auto i : vstd::range(8)) {
            mesh_builder.normal.emplace_back(color);
        }
        for (auto &i : luisa::span{mesh_builder.position}.subspan(idx)) {
            i *= scale;
            i += offset;
        }
        auto &triangle = mesh_builder.triangle_indices[0];
        auto tri_start = triangle.size();
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
        for (auto &i : luisa::span{triangle}.subspan(tri_start)) {
            i += idx;
        }
    };
    mesh_builder.triangle_indices.emplace_back();
    emplace(float3(1, 0.05, 0.05), float3(0.05, 0, 0), float3(1, 0, 0));
    emplace(float3(0.05, 1, 0.05), float3(0, 0.05, 0), float3(0, 1, 0));
    emplace(float3(0.05, 0.05, 1), float3(0, 0, 0.05), float3(0, 0, 1));
    luisa::vector<std::byte> gizmos_mesh;
    luisa::vector<uint> submesh_offset;
    mesh_builder.write_to(gizmos_mesh, submesh_offset);
    auto &render_device = RenderDevice::instance();
    _gizmos = new Gizmos{};
    _gizmos->data = render_device.lc_device().create_buffer<uint>(gizmos_mesh.size() / sizeof(uint));
    _gizmos->vertex_size = mesh_builder.vertex_count();
    render_device.lc_main_stream() << _gizmos->data.view().copy_from(gizmos_mesh.data());
}
bool WorldScene::draw_gizmos(
    bool dragging,
    GraphicsUtils *utils, uint2 click_pixel_pos, uint2 mouse_pos, uint2 window_size, double3 const &cam_pos, float cam_far_plane, Camera const &cam) {
    auto &click_mng = utils->render_settings().read_mut<ClickManager>();
    auto tr = _entities[0]->get_component<world::Transform>();

    auto transform_matrix = tr->trs();
    auto vertex_offset = _gizmos->vertex_size * sizeof(float3) / sizeof(uint);
    auto index_size = _gizmos->data.size() - vertex_offset * 2;
    auto clicked_result = click_mng.query_gizmos_result("test_gizmos");
    const float3 to_color{1, 0.84f, 0.0f};

    if (!dragging) {
        _gizmos->picked_face = uint(-1);
        if (clicked_result) {
            if (clicked_result->primitive_id < 12) {
                _gizmos->picked_face = 0;
            } else if (clicked_result->primitive_id < 24) {
                _gizmos->picked_face = 1;
            } else {
                _gizmos->picked_face = 2;
            }
        }
    }
    float3 from_color(-1);
    switch (_gizmos->picked_face) {
        case 0:
            from_color = float3(1, 0, 0);
            break;
        case 1:
            from_color = float3(0, 1, 0);
            break;
        case 2:
            from_color = float3(0, 0, 1);
            break;
    }
    click_mng.add_gizmos_requires(GizmosRequire{
        .name = "test_gizmos",
        .transform = make_float4x4(
            make_float4(transform_matrix[0]),
            make_float4(transform_matrix[1]),
            make_float4(transform_matrix[2]),
            make_float4(transform_matrix[3])),
        .pos_buffer = _gizmos->data.view(0, vertex_offset).as<float3>(),
        .color_buffer = _gizmos->data.view(vertex_offset, vertex_offset).as<float3>(),
        .triangle_buffer = _gizmos->data.view(vertex_offset * 2, index_size).as<Triangle>(),
        .clicked_pos = dragging ? uint2(-1) : mouse_pos,
        .from_color = from_color,
        .to_color = to_color});
    if (!dragging) {
        _gizmos->picking = false;
        return false;
    }
    _gizmos->picked_uv = (make_float2(mouse_pos) + 0.5f) / make_float2(window_size);
    _gizmos->picked_uv = clamp(_gizmos->picked_uv, float2(0), float2(1));
    if (!_gizmos->picking) {
        if (clicked_result) {
            LUISA_INFO("Clicking gizmos primitive id {}", clicked_result.value().primitive_id);
            auto global_pos = (transform_matrix * make_double4(make_double3(clicked_result->local_pos), 1.0));
            global_pos /= global_pos.w;
            _gizmos->to_cam_distance = static_cast<float>(std::min<double>(cam_far_plane, distance(global_pos.xyz(), cam_pos)));
            _gizmos->relative_pos = make_float3(global_pos.xyz() - transform_matrix[3].xyz());
            _gizmos->picking = true;
        }
    }
    if (!_gizmos->picking) return false;

    auto vp = cam.projection_matrix() * inverse(cam.local_to_world_matrix());
    auto inv_vp = inverse(vp);
    float2 uv = (make_float2(mouse_pos) + 0.5f) / make_float2(window_size);
    float4 proj_pos = make_float4(uv * 2.f - 1.f, 0.5f /*dummy depth*/, 1.f);
    auto dummy_world_pos = inv_vp * proj_pos;
    dummy_world_pos /= dummy_world_pos.w;
    auto ray_dir = normalize(dummy_world_pos.xyz() - make_float3(cam.position));
    auto target_pos = ray_dir * _gizmos->to_cam_distance + make_float3(cam.position);
    auto dest_pos = make_double3(target_pos - _gizmos->relative_pos);
    auto src_pos = tr->position();
    switch (_gizmos->picked_face) {
        case 0:
            src_pos.x = dest_pos.x;
            break;
        case 1:
            src_pos.y = dest_pos.y;
            break;
        case 2:
            src_pos.z = dest_pos.z;
            break;
    }
    tr->set_pos(src_pos, true);
    return true;
}
WorldScene::~WorldScene() {
    for (auto &i : _entities) {
        i->dispose();
    }
}
}// namespace rbc