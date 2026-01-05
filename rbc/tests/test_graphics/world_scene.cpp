#include "world_scene.h"
#include <rbc_world/components/transform.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/light.h>
#include <rbc_world/texture_loader.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_app/graphics_utils.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_core/serde.h>
#include <rbc_render/click_manager.h>
#include <rbc_world/importers/texture_importer_exr.h>
#include <rbc_world/importers/texture_importer_stb.h>
#include "jolt_component.h"
// #include <rbc_world/project.h>

#include <tracy_wrapper.h>

using rbc::ArchiveWriteJson;
using rbc::ArchiveReadJson;

namespace rbc {
void WorldScene::_init_scene(GraphicsUtils *utils) {
    RBCZoneScopedN("WorldScene::_init_scene");

    {
        RBCZoneScopedN("Load Cornell Box Mesh");
        cbox_mesh = world::create_object<world::MeshResource>();
        cbox_mesh->decode("cornell_box.obj");
        cbox_mesh->init_device_resource();
        utils->update_mesh_data(cbox_mesh->device_mesh(), false);// update through render-thread
    }
    {
        RBCZoneScopedN("Create Materials");
        _mats.resize(cbox_mesh->submesh_count());
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
        _mats[0] = basic_mat;
        _mats[1] = basic_mat;
        _mats[2] = basic_mat;
        _mats[3] = std::move(left_wall_mat);
        _mats[4] = std::move(right_wall_mat);
        _mats[5] = basic_mat;
        _mats[6] = basic_mat;
        _mats[7] = light_mat;
    }
    _mats[5] = nullptr;
    {
        auto entity = _entities.emplace_back(world::create_object<world::Entity>());
        auto transform = entity->add_component<world::TransformComponent>();
        transform->set_pos(double3(0, -1, 2), true);
        auto rot = quaternion(
            make_float3x3(
                -1, 0, 0,
                0, 1, 0,
                0, 0, -1));
        transform->set_rotation(rot, true);
        auto render = entity->add_component<world::RenderComponent>();
        render->start_update_object(_mats, cbox_mesh);
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
    quad_mesh = world::create_object<world::MeshResource>();
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> quad_bytes;
    mesh_builder.write_to(quad_bytes, submesh_offsets);
    quad_mesh->create_empty({}, std::move(submesh_offsets), mesh_builder.vertex_count(), mesh_builder.indices_count() / 3, mesh_builder.uv_count(), mesh_builder.contained_normal(), mesh_builder.contained_tangent());
    auto s = quad_bytes.size_bytes();
    *quad_mesh->host_data() = std::move(quad_bytes);
    quad_mesh->init_device_resource();
    utils->update_mesh_data(quad_mesh->device_mesh(), false);// update through render-thread

    {
        world::ExrTextureImporter exr_importer;
        world::StbTextureImporter stb_importer;

        RBCZoneScopedN("Load Textures");
        world::TextureLoader tex_loader;
        // tex = tex_loader.decode_texture(
        //     "test_grid.png",
        //     16,
        //     true);
        tex = world::create_object<world::TextureResource>();
        stb_importer.import(tex, &tex_loader, "test_grid.png", 16, true);
        // skybox = tex_loader.decode_texture(
        //     "sky.exr",
        //     1,
        //     false);
        skybox = world::create_object<world::TextureResource>();
        exr_importer.import(skybox, &tex_loader, "sky.exr", 1, false);
        // write guid
        {
            RBCZoneScopedN("Write Sky GUID");
            auto sky_guid = skybox->guid();
            BinaryFileWriter file_writer{"test_scene/sky_guid.txt"};
            auto sky_str = sky_guid.to_string();
            file_writer.write({(std::byte *)sky_str.data(),
                               sky_str.size()});
        }

        tex_loader.finish_task();
        // TODO: transform from regular tex to vt need reload device-image
        // tex->pack_to_tile();
        tex->init_device_resource();
        // utils->update_texture(tex->get_image());
        skybox->init_device_resource();
    }

    rbc::RC<DeviceImage> image{skybox->get_image()};
    utils->render_plugin()->update_skybox(image);

    utils->update_texture(skybox->get_image());// update through render-thread

    auto quad_entity = _entities.emplace_back(world::create_object<world::Entity>());
    auto quad_trans = quad_entity->add_component<world::TransformComponent>();
    auto quad_render = quad_entity->add_component<world::RenderComponent>();
    quad_trans->set_pos(make_double3(0, 0, 4), true);
    quad_trans->set_scale(double3(10, 10, 10), true);
    auto quad_mat = world::create_object<world::MaterialResource>();
    auto quad_mat_str = luisa::format(R"({{"type": "pbr", "base_albedo_tex": "{}"}})", tex->guid().to_base64());
    quad_mat->load_from_json(quad_mat_str);
    auto mat_start_idx = _mats.size();
    _mats.emplace_back(quad_mat);
    auto mats = luisa::span{_mats}.subspan(mat_start_idx, 1);
    quad_render->start_update_object(mats, quad_mesh);
}
void WorldScene::_write_scene() {
    // save scene
    vstd::HashMap<vstd::Guid> saved;
    auto write_file = [&](world::Resource *res) {
        if (!res || !saved.try_emplace(res->guid()).second) return;
        res->save_to_path();
        world::register_resource(res);
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
    ArchiveWriteJson scene_adapter(scene_ser);
    world::ObjSerialize ser{scene_adapter};
    for (auto &i : _entities) {
        scene_adapter.start_object();
        i->serialize_meta(ser);
        scene_adapter.end_object();
    }
    BinaryFileWriter file_writer(luisa::to_string(entities_path));
    file_writer.write(scene_ser.write_to());
}
WorldScene::WorldScene(GraphicsUtils *utils) {
    auto &render_device = RenderDevice::instance();
    auto runtime_dir = render_device.lc_ctx().runtime_directory();
    luisa::filesystem::path meta_dir{"test_scene"};
    scene_root_dir = runtime_dir / meta_dir;
    entities_path = scene_root_dir / "scene.rbc";

    // write a demo scene
    if (!luisa::filesystem::exists(scene_root_dir) || luisa::filesystem::is_empty(scene_root_dir)) {
        luisa::filesystem::create_directories(scene_root_dir);
        world::init_world(scene_root_dir);
        world::load_all_resources_from_meta();
        _init_scene(utils);

    } else {
        // load demo scene
        world::init_world(scene_root_dir);// open project folder
        world::load_all_resources_from_meta();
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
        ArchiveReadJson read_adapter(entitie_deser);
        uint64_t size = entitie_deser.last_array_size();
        _entities.reserve(size);
        for (auto i : vstd::range(size)) {
            auto e = _entities.emplace_back(world::create_object<world::Entity>());
            read_adapter.start_object();
            e->deserialize_meta(world::ObjDeSerialize{read_adapter});
            read_adapter.end_scope();
        }
        for (auto &i : _entities) {
            auto render = i->get_component<world::RenderComponent>();
            if (render) {
                render->start_update_object();
            }
        }
        // wait skybox
        {
            auto wait_skybox = [&]() -> rbc::coroutine {
                co_await skybox->await_loading();
            }();
            while (!wait_skybox.done()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                wait_skybox.resume();
            }
        }
        rbc::RC<DeviceImage> image{skybox->get_image()};
        utils->render_plugin()->update_skybox(image);
    }
    _set_gizmos();
    // _init_physics(utils);
    _init_skinning(utils);
    // {
    //     world::Project project{
    //         "test_scene",
    //         "test_scene_meta",
    //         "test_scene_binary",
    //         "test_scene_meta/.database"};
    //     project.scan_project();
    //     project.db().read_columns_with(
    //         "RBC_FILE_META"sv,
    //         [&](SqliteCpp::ColumnValue &&column) {
    //             luisa::string spaces;
    //             spaces.resize(32 - column.name.size(), ' ');
    //             LUISA_INFO("Name: {}{}Value: {}", column.name, spaces, column.value);
    //         });
    // }
}
void WorldScene::_init_physics(GraphicsUtils *utils) {
    MeshBuilder cube_mesh_builder;
    _create_cube(cube_mesh_builder, float3(-0.5f), float3(1));
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> cube_bytes;
    cube_mesh_builder.write_to(cube_bytes, submesh_offsets);
    // create static origin mesh
    auto mat1 = R"({"type": "pbr", "specular_roughness": 0.8, "weight_metallic": 0.3, "base_albedo": [0.140, 0.450, 0.091]})"sv;

    physics_mat = RC<world::MaterialResource>{world::create_object<world::MaterialResource>()};
    {
        physics_box_mesh = world::create_object<world::MeshResource>();
        physics_box_mesh->create_empty({}, std::move(submesh_offsets), cube_mesh_builder.vertex_count(), cube_mesh_builder.indices_count() / 3, cube_mesh_builder.uv_count(), cube_mesh_builder.contained_normal(), cube_mesh_builder.contained_tangent());

        *physics_box_mesh->host_data() = std::move(cube_bytes);
        physics_box_mesh->init_device_resource();
        utils->update_mesh_data(physics_box_mesh->device_mesh(), false);// update through render-thread
    }
    // floor
    {
        physics_floor_entity = world::create_object<world::Entity>();
        auto tr = physics_floor_entity->add_component<world::TransformComponent>();
        auto render = physics_floor_entity->add_component<world::RenderComponent>();
        auto jolt = physics_floor_entity->add_component<world::JoltComponent>();
        jolt->init(true);
        render->start_update_object({}, physics_box_mesh.get());
    }
    // box
    {
        physics_box_entity = world::create_object<world::Entity>();
        auto tr = physics_box_entity->add_component<world::TransformComponent>();
        auto render = physics_box_entity->add_component<world::RenderComponent>();
        auto jolt = physics_box_entity->add_component<world::JoltComponent>();
        jolt->init(false);
        auto mats = {physics_mat};
        physics_mat->load_from_json(mat1);
        render->start_update_object(mats, physics_box_mesh.get());
    }
}
void WorldScene::_init_skinning(GraphicsUtils *utils) {
    auto &render_device = RenderDevice::instance();
    auto &device = render_device.lc_device();
    test_bones = device.create_buffer<DualQuaternion>(2);
    MeshBuilder cube_mesh_builder;
    _create_cube(cube_mesh_builder, float3(), float3(1));
    skinning_mesh = world::create_object<world::MeshResource>();
    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> cube_bytes;
    cube_mesh_builder.write_to(cube_bytes, submesh_offsets);
    // create static origin mesh
    {
        skinning_origin_mesh = world::create_object<world::MeshResource>();
        skinning_origin_mesh->create_empty({}, std::move(submesh_offsets), cube_mesh_builder.vertex_count(), cube_mesh_builder.indices_count() / 3, cube_mesh_builder.uv_count(), cube_mesh_builder.contained_normal(), cube_mesh_builder.contained_tangent());

        *skinning_origin_mesh->host_data() = std::move(cube_bytes);
        skinning_origin_mesh->add_property("skinning_weight_index", cube_mesh_builder.vertex_count() * 2 * sizeof(uint));
        skinning_origin_mesh->init_device_resource();
        utils->update_mesh_data(skinning_origin_mesh->device_mesh(), false);// update through render-thread
    }
    auto skinning_weight_index = skinning_origin_mesh->get_or_create_property_buffer("skinning_weight_index").as<uint>();
    // create skinning mesh
    {
        // assign skinning mesh as the morphing instance of origin mesh
        skinning_mesh->create_as_morphing_instance(skinning_origin_mesh.get());
        skinning_mesh->init_device_resource();
    }
    luisa::vector<uint> weight_and_index_host;
    weight_and_index_host.push_back_uninitialized(skinning_weight_index.size());
    luisa::span<float> weights{
        (float *)weight_and_index_host.data(),
        weight_and_index_host.size() / 2};
    luisa::span<uint> indices{
        weight_and_index_host.data() + (weight_and_index_host.size() / 2),
        weight_and_index_host.size() / 2};
    for (auto &i : weights) {
        i = 1.0f;// set all weight to 1.0 for test
    }
    for (auto i : vstd::range(indices.size())) {
        indices[i] = (i < indices.size() / 2) ? 0 : 1;// index to bone 0 and bone 1
    }
    render_device.lc_main_stream() << skinning_weight_index.copy_from(weight_and_index_host.data());
    skinning_entity = world::create_object<world::Entity>();
    auto tr = skinning_entity->add_component<world::TransformComponent>();
    tr->set_pos(double3(0, 2, 1), false);
    auto render = skinning_entity->add_component<world::RenderComponent>();
    render->start_update_object({}, skinning_mesh.get());
}
void WorldScene::_create_cube(MeshBuilder &mesh_builder, float3 offset, float3 scale) {
    auto idx = mesh_builder.position.size();
    mesh_builder.position.push_back(float3(0.0f, 0.0f, 0.0f));// 0: Left Bottom Back
    mesh_builder.position.push_back(float3(0.0f, 0.0f, 1.0f));// 1: Left Bottom Front
    mesh_builder.position.push_back(float3(1.0f, 0.0f, 0.0f));// 2: Right Buttom Back
    mesh_builder.position.push_back(float3(1.0f, 0.0f, 1.0f));// 3: Right Buttom Front
    mesh_builder.position.push_back(float3(0.0f, 1.0f, 0.0f));// 4: Left Up Back
    mesh_builder.position.push_back(float3(0.0f, 1.0f, 1.0f));// 5: Left Up Front
    mesh_builder.position.push_back(float3(1.0f, 1.0f, 0.0f));// 6: Right Up Back
    mesh_builder.position.push_back(float3(1.0f, 1.0f, 1.0f));// 7: Right Up Front

    for (auto &i : luisa::span{mesh_builder.position}.subspan(idx)) {
        i *= scale;
        i += offset;
    }
    if (mesh_builder.triangle_indices.empty())
        mesh_builder.triangle_indices.emplace_back();
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
}
void WorldScene::_set_gizmos() {
    MeshBuilder mesh_builder;
    auto emplace = [&](float3 scale, float3 offset, float3 color) {
        _create_cube(mesh_builder, offset, scale);
        for (auto i : vstd::range(8)) {
            mesh_builder.normal.emplace_back(color);
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
    if (_entities.empty())
        return false;
    RBCZoneScopedN("WorldScene::draw_gizmos");

    auto &click_mng = utils->render_settings().read_mut<ClickManager>();
    auto tr = _entities[0]->get_component<world::TransformComponent>();

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
    double2 uv = (make_double2(mouse_pos) + 0.5) / make_double2(window_size);
    auto proj_pos = make_double4(uv * 2. - 1., 0.5 /*dummy depth*/, 1.);
    auto dummy_world_pos = inv_vp * proj_pos;
    dummy_world_pos /= dummy_world_pos.w;
    auto ray_dir = normalize(dummy_world_pos.xyz() - make_double3(cam.position));
    auto target_pos = ray_dir * (double)_gizmos->to_cam_distance + make_double3(cam.position);
    auto dest_pos = make_double3(target_pos - make_double3(_gizmos->relative_pos));
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
    _write_scene();
    skybox.reset();
    tex.reset();
    _mats.clear();
    for (auto &i : _entities) {
        i->delete_this();
    }
    skinning_entity.reset();
    skinning_mesh.reset();
    skinning_origin_mesh.reset();
    physics_mat.reset();
    physics_floor_entity.reset();
    physics_box_entity.reset();
    physics_box_mesh.reset();
    world::destroy_world();
}
void WorldScene::tick_skinning(GraphicsUtils *utils, float delta_time) {
    if (physics_box_entity) {
        world::JoltComponent::update_step(min(delta_time, 1 / 60.0f));
        physics_box_entity->get_component<world::JoltComponent>()->update_pos();
    }
    if (_entities.empty())
        return;
    static Clock clk;
    auto &sm = SceneManager::instance();
    auto &cmdlist = RenderDevice::instance().lc_main_cmd_list();
    // upload bones to gpu
    luisa::vector<DualQuaternion> bones{2};
    auto time = clk.toc() * 1e-3 * 3;
    bones[0] = encode_dual_quaternion(float3(sin(time), -0.5, cos(time)), Quaternion{});
    bones[1] = encode_dual_quaternion(float3(sin(time + pi * 0.5f), 0.5, cos(time + pi * 0.5f)), Quaternion{});
    cmdlist << test_bones.view().copy_from(bones.data());
    sm.dispose_after_commit(std::move(bones));
    // skinning
    utils->update_skinning(
        skinning_mesh.get(),
        test_bones);
}
}// namespace rbc