#include <rbc_graphics/lights.h>
#include <rbc_graphics/materials.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
#include <material/mats.inl>
namespace light_detail {
static Lights *_inst = nullptr;
}// namespace light_detail
Lights::~Lights() {
    dispose();
    if (SceneManager::instance_ptr() != nullptr)
        SceneManager::instance_ptr()->remove_before_render_event("_lights_tick");
    if (light_detail::_inst == this) {
        light_detail::_inst = nullptr;
    }
}
// run before render
void Lights::scene_manager_tick() {
    _before_render_mtx.lock();
    auto before_render_funcs = std::move(_before_render_funcs);
    _before_render_mtx.unlock();
    for (auto &i : before_render_funcs) {
        if (i()) {
            _before_render_mtx.lock();
            _before_render_funcs.emplace_back(std::move(i));
            _before_render_mtx.unlock();
        }
    }
}

Lights::Lights()
    : mesh_light_accel() {
    if (light_detail::_inst != nullptr) {
        LUISA_WARNING("Multiple light instance not allowed.");
        return;
    }
    light_detail::_inst = this;
    Device &device = RenderDevice::instance().lc_device();
    Stream &stream = RenderDevice::instance().lc_main_stream();
    auto &scene = SceneManager::instance();
    scene.add_before_render_event("_lights_tick", this);
    CommandList cmdlist;
    auto commit_cmdlist = vstd::scope_exit([&]() {
        stream << cmdlist.commit();
    });
    ///////////// Load area-light quad vertex & index
    auto data_buffer = device.create_buffer<uint>((sizeof(float3) * 4 + sizeof(float2) * 4 + sizeof(Triangle) * 2) / sizeof(uint));
    auto vert_buffer = scene.host_upload_buffer().allocate_upload_buffer<uint>(data_buffer.size());
    vector<uint> data_arr;
    data_arr.push_back_uninitialized(data_buffer.size());
    auto vert = reinterpret_cast<float3 *>(data_arr.data());
    auto uv = reinterpret_cast<float2 *>(vert + 4);
    auto tri = reinterpret_cast<Triangle *>(uv + 4);

    vert[0] = float3(-0.5, -0.5, 0);
    vert[1] = float3(-0.5, 0.5, 0);
    vert[2] = float3(0.5, -0.5, 0);
    vert[3] = float3(0.5, 0.5, 0);
    uv[0] = float2(0, 0);
    uv[1] = float2(0, 1);
    uv[2] = float2(1, 0);
    uv[3] = float2(1, 1);
    tri[0] = Triangle{0, 2, 1};
    tri[1] = Triangle{1, 2, 3};
    std::memcpy(vert_buffer.mapped_ptr(), data_arr.data(), data_arr.size_bytes());
    cmdlist << data_buffer.view().copy_from(vert_buffer.view);
    ///////////// Build area-light quad mesh BLAS
    quad_mesh = scene.mesh_manager().load_mesh(
        scene.bindless_allocator(),
        cmdlist,
        scene.host_upload_buffer(),
        std::move(data_buffer),
        AccelOption{.allow_compaction = false, .allow_update = false},
        4,
        false,
        false,
        1, {});
    quad_mesh->bbox_requests = new MeshManager::BBoxRequest();
    quad_mesh->bbox_requests->finished = true;
    quad_mesh->bbox_requests->mesh_data = quad_mesh;
    quad_mesh->bbox_requests->bounding_box.emplace_back(
        AABB{
            .packed_min = {-0.5f, -0.5f, 0},
            .packed_max = {0.5f, 0.5f, 0},
        });

    luisa::vector<float3> sphere_vertices;
    luisa::vector<uint> sphere_triangles;
    {
        LightAccel::generate_sphere_mesh(sphere_vertices, sphere_triangles);
        auto data_buffer = device.create_buffer<uint>((sphere_triangles.size_bytes() + sphere_vertices.size_bytes()) / sizeof(uint));
        auto vert_buffer = scene.host_upload_buffer().allocate_upload_buffer<uint>(data_buffer.size());
        auto ptr = reinterpret_cast<std::byte *>(vert_buffer.mapped_ptr());
        std::memcpy(ptr, sphere_vertices.data(), sphere_vertices.size_bytes());
        ptr += sphere_vertices.size_bytes();
        std::memcpy(ptr, sphere_triangles.data(), sphere_triangles.size_bytes());
        cmdlist << data_buffer.view().copy_from(vert_buffer.view);
        point_mesh = scene.mesh_manager().load_mesh(
            scene.bindless_allocator(),
            cmdlist,
            scene.host_upload_buffer(),
            std::move(data_buffer),
            AccelOption{.allow_compaction = false, .allow_update = false},
            sphere_vertices.size(),
            false, false, 0, {});
        point_mesh->bbox_requests = new MeshManager::BBoxRequest();
        point_mesh->bbox_requests->finished = true;
        point_mesh->bbox_requests->mesh_data = point_mesh;
        point_mesh->bbox_requests->bounding_box.emplace_back(
            AABB{
                .packed_min = {-1, -1, -1},
                .packed_max = {1, 1, 1},
            });
    }
    luisa::vector<float3> disk_vertices;
    luisa::vector<uint> disk_triangles;
    {
        const uint disk_split_triangle = 64;
        disk_vertices.reserve(disk_split_triangle + 2);
        disk_triangles.reserve(3 * disk_split_triangle);
        auto get_vert = [](float weight) {
            auto rad = weight * luisa::pi * 2.f;
            return float3(cos(rad), sin(rad), 0.f);
        };
        // center
        disk_vertices.emplace_back(float3(0));
        // first
        disk_vertices.emplace_back(get_vert(0.f));
        for (auto i : vstd::range(disk_split_triangle)) {
            auto last_vert_idx = disk_vertices.size() - 1;
            disk_vertices.emplace_back(get_vert(float(i + 1) / float(disk_split_triangle)));
            disk_triangles.emplace_back(0);
            disk_triangles.emplace_back(last_vert_idx);
            disk_triangles.emplace_back(disk_vertices.size() - 1);
        }
        auto data_buffer = device.create_buffer<uint>((disk_triangles.size_bytes() + disk_vertices.size_bytes()) / sizeof(uint));
        auto vert_buffer = scene.host_upload_buffer().allocate_upload_buffer<uint>(data_buffer.size());
        auto ptr = reinterpret_cast<std::byte *>(vert_buffer.mapped_ptr());
        std::memcpy(ptr, disk_vertices.data(), disk_vertices.size_bytes());
        ptr += disk_vertices.size_bytes();
        std::memcpy(ptr, disk_triangles.data(), disk_triangles.size_bytes());
        cmdlist << data_buffer.view().copy_from(vert_buffer.view);
        disk_mesh = scene.mesh_manager().load_mesh(
            scene.bindless_allocator(),
            cmdlist,
            scene.host_upload_buffer(),
            std::move(data_buffer),
            AccelOption{.allow_compaction = false, .allow_update = false},
            disk_vertices.size(),
            false, false, 0, {});
        disk_mesh->bbox_requests = new MeshManager::BBoxRequest();
        disk_mesh->bbox_requests->finished = true;
        disk_mesh->bbox_requests->mesh_data = disk_mesh;
        disk_mesh->bbox_requests->bounding_box.emplace_back(
            AABB{
                .packed_min = {-0.5f, -0.5f, 0},
                .packed_max = {0.5f, 0.5f, 0},
            });
    }
    // ///////////// Add emission material type
    scene.mat_manager().emplace_mat_type<material::PolymorphicMaterial, rbc::material::Unlit>(scene.bindless_allocator(), 4096);
}

uint Lights::add_point_light(
    CommandList &cmdlist,
    float3 center,
    float radius,
    float3 emission,
    bool visible) {
    auto &scene = SceneManager::instance();
    LightAccel::PointLight point_light;
    point_light.sphere = make_float4(center, radius);
    point_light.radiance = LightAccel::PackFloat3(emission);
    point_light.mis_weight = visible ? 1 : -1;
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    auto mat = scene.mat_manager().emplace_mat_instance<material::PolymorphicMaterial>(
        mat_inst,
        cmdlist,
        scene.bindless_allocator(),
        scene.buffer_uploader(),
        scene.dispose_queue());
    uint data_index = 0;
    if (point_lights.removed_list.empty()) {
        point_lights.light_data.emplace_back();
        data_index = point_lights.light_data.size() - 1;
    } else {
        data_index = point_lights.removed_list.back();
        point_lights.removed_list.pop_back();
    }
    auto light_id = scene.light_accel().emplace(
        cmdlist,
        scene,
        point_light,
        data_index);
    auto get_tlas_id = [&]() {
        if (visible) {
            return scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                point_mesh,
                luisa::span{&mat, 1},
                translation(center) * scaling(radius),
                0xffu,
                true,
                light_id,
                LightAccel::t_PointLight);
        } else {
            return ~0u;
        }
    };
    point_lights.light_data[data_index] = LightData{
        // tlas_id
        /////////// Add TLAS instance
        get_tlas_id(),
        // mat_code
        mat,
        // light_id
        light_id};

    return data_index;
}
uint Lights::add_spot_light(
    CommandList &cmdlist,
    float3 center,
    float radius,
    float3 emission,
    float3 forward_dir,
    float angle,
    float small_angle,
    float angle_atten_pow,
    bool visible) {
    LightAccel::SpotLight spot_light;
    spot_light.sphere = make_float4(center, radius);
    spot_light.radiance = LightAccel::PackFloat3(emission);
    spot_light.forward_dir = LightAccel::PackFloat3(forward_dir);
    spot_light.angle_radian = angle * 0.5f;
    spot_light.small_angle_radian = small_angle * 0.5f;
    spot_light.angle_atten_power = angle_atten_pow;
    spot_light.mis_weight = visible ? 1 : -1;
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    auto &scene = SceneManager::instance();
    auto mat = scene.mat_manager().emplace_mat_instance<material::PolymorphicMaterial>(
        mat_inst,
        cmdlist,
        scene.bindless_allocator(),
        scene.buffer_uploader(),
        scene.dispose_queue());
    uint data_index = 0;
    if (spot_lights.removed_list.empty()) {
        spot_lights.light_data.emplace_back();
        data_index = spot_lights.light_data.size() - 1;
    } else {
        data_index = spot_lights.removed_list.back();
        spot_lights.removed_list.pop_back();
    }
    auto light_id = scene.light_accel().emplace(
        cmdlist,
        scene,
        spot_light,
        data_index);
    auto get_tlas_id = [&]() {
        if (visible) {
            return scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                point_mesh,
                luisa::span{&mat, 1},
                translation(center) * scaling(radius),
                0xffu,
                true,
                light_id,
                LightAccel::t_SpotLight);
        } else {
            return ~0u;
        }
    };
    spot_lights.light_data[data_index] = LightData{
        // tlas_id
        /////////// Add TLAS instance
        get_tlas_id(),
        // mat_code
        mat,
        // light_id
        light_id};

    return data_index;
}

uint Lights::add_area_light(
    CommandList &cmdlist,
    float4x4 local_to_world,
    float3 emission,
    Image<float> const *emission_img,
    Sampler const *sampler,
    bool visible) {
    auto &scene = SceneManager::instance();

    float2 area{
        distance((local_to_world * float4(-0.5f, 0, 0, 1)).xyz(), (local_to_world * float4(0.5f, 0, 0, 1)).xyz()),
        distance((local_to_world * float4(0, -0.5f, 0, 1)).xyz(), (local_to_world * float4(0, 0.5f, 0, 1)).xyz())};
    ///////////// Make light accel instance data
    LightAccel::AreaLight area_light;
    area_light.area = area.x * area.y;
    area_light.mis_weight = visible ? 1 : -1;
    if (emission_img) {
        area_light.emission_tex_id = scene.bindless_manager().enqueue_image(*emission_img, sampler ? *sampler : Sampler::anisotropic_edge());
    } else {
        area_light.emission_tex_id = ~0u;
    }
    area_light.radiance = LightAccel::PackFloat3(emission);
    area_light.transform = local_to_world;

    ///////////// Make light emission material
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    mat_inst.tex.index = area_light.emission_tex_id;
    auto mat = scene.mat_manager().emplace_mat_instance<material::PolymorphicMaterial>(
        mat_inst,
        cmdlist,
        scene.bindless_allocator(),
        scene.buffer_uploader(),
        scene.dispose_queue());
    uint data_index = 0;
    if (area_lights.removed_list.empty()) {
        this->area_lights.light_data.emplace_back();
        data_index = this->area_lights.light_data.size() - 1;
    } else {
        data_index = area_lights.removed_list.back();
        area_lights.removed_list.pop_back();
    }
    ///////////// Add light accel
    auto light_id = scene.light_accel().emplace(
        cmdlist,
        scene,
        area_light,
        data_index);
    auto get_tlas_id = [&]() {
        if (visible) {
            return scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                quad_mesh,
                luisa::span{&mat, 1},
                local_to_world,
                0xffu,
                true,
                light_id,
                LightAccel::t_AreaLight// Area light
            );
        } else {
            return ~0u;
        }
    };
    this->area_lights.light_data[data_index] = LightData{
        // tlas_id
        /////////// Add TLAS instance
        get_tlas_id(),
        // mat_code
        mat,
        // light_id
        light_id};
    return data_index;
}

uint Lights::add_disk_light(
    CommandList &cmdlist,
    float3 center,
    float radius,
    float3 emission,
    float3 forward_dir,
    bool visible) {
    auto &scene = SceneManager::instance();
    LightAccel::DiskLight disk_light;
    disk_light.forward_dir = LightAccel::PackFloat3(forward_dir);
    disk_light.radiance = LightAccel::PackFloat3(emission);
    disk_light.position = LightAccel::PackFloat3(center);
    disk_light.area = radius * radius * luisa::pi;
    disk_light.mis_weight = visible ? 1 : -1;
    ///////////// Make light emission material
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    mat_inst.tex.index = -1;
    auto mat = scene.mat_manager().emplace_mat_instance<material::PolymorphicMaterial>(
        mat_inst,
        cmdlist,
        scene.bindless_allocator(),
        scene.buffer_uploader(),
        scene.dispose_queue());
    uint data_index = 0;
    if (disk_lights.removed_list.empty()) {
        this->disk_lights.light_data.emplace_back();
        data_index = this->disk_lights.light_data.size() - 1;
    } else {
        data_index = disk_lights.removed_list.back();
        disk_lights.removed_list.pop_back();
    }
    ///////////// Add light accel
    auto light_id = scene.light_accel().emplace(
        cmdlist,
        scene,
        disk_light,
        data_index);
    auto get_tlas_id = [&]() {
        if (visible) {
            float sign = copysign(1.0f, forward_dir.z);
            const float a = -(1.f / (sign + forward_dir.z));
            const float b = forward_dir.x * forward_dir.y * a;
            float3 tangent = float3(1.0f + sign * forward_dir.x * forward_dir.x * a, sign * b, -sign * forward_dir.x);
            float3 bitangent = float3(b, sign + forward_dir.y * forward_dir.y * a, -forward_dir.y);
            float4x4 local_to_world;
            local_to_world.cols[0] = make_float4(tangent, 0);
            local_to_world.cols[1] = make_float4(bitangent, 0);
            local_to_world.cols[2] = make_float4(forward_dir, 0);
            local_to_world.cols[3] = make_float4(center, 1);
            local_to_world = local_to_world * scaling(radius);

            return scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                disk_mesh,
                luisa::span{&mat, 1},
                local_to_world,
                0xffu,
                true,
                light_id,
                LightAccel::t_DiskLight// Disk light
            );
        } else {
            return ~0u;
        }
    };
    this->disk_lights.light_data[data_index] = LightData{
        // tlas_id
        /////////// Add TLAS instance
        get_tlas_id(),
        // mat_code
        mat,
        // light_id
        light_id};
    return data_index;
}

void Lights::add_tick(vstd::function<bool()> &&func) {
    std::lock_guard lck{_before_render_mtx};
    _before_render_funcs.emplace_back(std::move(func));
}

uint Lights::add_mesh_light_sync(
    CommandList &cmdlist,
    RC<DeviceMesh> const &device_mesh,
    float4x4 local_to_world,
    luisa::span<float const> material_emissions,
    luisa::span<MatCode const> material_codes) {
    auto &scene = SceneManager::instance();
    uint data_index = 0;
    if (mesh_lights.removed_list.empty()) {
        mesh_lights.light_data.emplace_back();
        data_index = mesh_lights.light_data.size() - 1;
    } else {
        data_index = mesh_lights.removed_list.back();
        mesh_lights.removed_list.pop_back();
    }
    auto &v = mesh_lights.light_data[data_index];
    v.device_mesh = device_mesh;
    device_mesh->wait_finished();
    v.tlas_light_id = scene.light_accel()._get_next_meshlight_index();
    v.tlas_id = scene.accel_manager().emplace_mesh_instance(
        cmdlist, scene.host_upload_buffer(), scene.buffer_allocator(),
        scene.buffer_uploader(), scene.dispose_queue(),
        device_mesh->mesh_data(),
        material_codes,
        local_to_world,
        0xffu,
        true,
        v.tlas_light_id,
        LightAccel::t_MeshLight);
    auto &&mesh_data = v.device_mesh->mesh_data();
    auto mesh_host_data = v.device_mesh->host_data();
    auto host_result = MeshLightAccel::build_bvh(
        local_to_world,
        span{(float3 *)mesh_host_data.data(), mesh_data->meta.vertex_count},
        span{(Triangle *)(mesh_host_data.data() + mesh_data->meta.tri_byte_offset), mesh_data->triangle_size},
        mesh_data->submesh_offset,
        material_emissions);

    if (host_result.buffer_size == 0) {
        v.light_id = ~0u;
        v.blas_heap_idx = ~0u;
    } else {
        mesh_light_accel.create_or_update_blas(cmdlist, v.blas_buffer, host_result.buffer_size, std::move(host_result.nodes));
        v.blas_heap_idx = scene.bindless_allocator().allocate_buffer(v.blas_buffer);

        // TODO: blas_heap_idx
        LightAccel::MeshLight mesh_light{
            local_to_world,
            {host_result.bounding.min.x, host_result.bounding.min.y, host_result.bounding.min.z},
            v.blas_heap_idx,
            {host_result.bounding.max.x, host_result.bounding.max.y, host_result.bounding.max.z},
            v.tlas_id,
            host_result.contribute,
            1.f};
        v.light_id = scene.light_accel().emplace(cmdlist, scene, mesh_light, data_index);
    }
    return data_index;
}

void Lights::update_spot_light(
    CommandList &cmdlist,
    uint light_index,
    float3 center,
    float radius,
    float3 emission,
    float3 forward_dir,
    float angle,      // radian
    float small_angle,// radian
    float angle_atten_pow,
    bool visible) {
    auto &scene = SceneManager::instance();
    auto &data = spot_lights.light_data[light_index];
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    if (data.tlas_id == ~0u) {
        if (visible) {
            data.tlas_id = scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                point_mesh,
                luisa::span{&data.mat_code, 1},
                translation(center) * scaling(radius),

                0xffu,
                true,
                data.light_id,
                LightAccel::t_SpotLight);
        }
    } else {
        if (visible) {
            scene.accel_manager().set_mesh_instance(
                cmdlist,
                scene.buffer_uploader(),
                data.tlas_id,
                translation(center) * scaling(radius),
                0xffu,
                true);
        } else {
            scene.accel_manager().remove_mesh_instance(
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                data.tlas_id);
            data.tlas_id = ~0u;
        }
    }
    /////////// Update emission material
    scene.mat_manager().set_mat_instance(
        data.mat_code,
        scene.buffer_uploader(),
        luisa::span{reinterpret_cast<std::byte const *>(&mat_inst), sizeof(mat_inst)});

    LightAccel::SpotLight spot_light;
    spot_light.sphere = make_float4(center, radius);
    spot_light.radiance = LightAccel::PackFloat3(emission);

    spot_light.forward_dir = LightAccel::PackFloat3(forward_dir);
    spot_light.angle_radian = angle * 0.5f;
    spot_light.small_angle_radian = small_angle * 0.5f;
    spot_light.angle_atten_power = angle_atten_pow;
    spot_light.mis_weight = visible ? 1 : -1;
    /////////// Update light accel
    scene.light_accel().update(
        scene,
        data.light_id,
        spot_light);
}

void Lights::update_mesh_light_sync(
    CommandList &cmdlist,
    uint data_index,
    float4x4 local_to_world,
    luisa::span<float const> material_emissions,
    luisa::span<MatCode const> material_codes,
    RC<DeviceMesh> const *new_mesh) {
    auto &scene = SceneManager::instance();
    auto &v = mesh_lights.light_data[data_index];
    if (new_mesh) {
        v.device_mesh = *new_mesh;
    }
    v.device_mesh->wait_finished();
    auto mesh_host_data = v.device_mesh->host_data();
    if (mesh_host_data.empty()) [[unlikely]] {
        LUISA_ERROR("Mesh light must have host data.");
    }
    auto &&mesh_data = v.device_mesh->mesh_data();
    auto host_result = MeshLightAccel::build_bvh(
        local_to_world,
        span{(float3 *)mesh_host_data.data(), mesh_data->meta.vertex_count},
        span{(Triangle *)(mesh_host_data.data() + mesh_data->meta.tri_byte_offset), mesh_data->triangle_size},
        mesh_data->submesh_offset,
        material_emissions);
    if (host_result.buffer_size == 0) {
        if (v.blas_heap_idx != ~0u) {
            scene.bindless_allocator().deallocate_buffer(v.blas_heap_idx);
            v.blas_heap_idx = ~0u;
        }
    } else {
        auto new_buffer = mesh_light_accel.create_or_update_blas(cmdlist, v.blas_buffer, host_result.buffer_size, std::move(host_result.nodes));
        if (new_buffer) {
            if (v.blas_heap_idx != ~0u) {
                scene.bindless_allocator().deallocate_buffer(v.blas_heap_idx);
            }
            v.blas_heap_idx = scene.bindless_allocator().allocate_buffer(v.blas_buffer);
        }
    }
    if (new_mesh) {
        scene.accel_manager().set_mesh_instance(
            v.tlas_id,
            cmdlist,
            scene.host_upload_buffer(),
            scene.buffer_allocator(),
            scene.buffer_uploader(),
            scene.dispose_queue(),
            v.device_mesh->mesh_data(),
            material_codes,
            local_to_world,
            0xffu,
            true,
            v.tlas_light_id,
            LightAccel::t_MeshLight,
            true);
    } else {
        scene.accel_manager().set_mesh_instance(
            cmdlist,
            scene.buffer_uploader(),
            v.tlas_id,
            local_to_world,
            0xffu,
            true);
    }
    if (v.blas_heap_idx != ~0u) {
        LightAccel::MeshLight mesh_light{
            local_to_world,
            {host_result.bounding.min.x, host_result.bounding.min.y, host_result.bounding.min.z},
            v.blas_heap_idx,
            {host_result.bounding.max.x, host_result.bounding.max.y, host_result.bounding.max.z},
            v.tlas_id,
            host_result.contribute,
            1.f};
        if (v.light_id != ~0u) {
            scene.light_accel().update(
                scene,
                v.light_id,
                mesh_light);
        } else {
            v.light_id = scene.light_accel().emplace(
                cmdlist,
                scene,
                mesh_light,
                data_index);
        }
    } else {
        if (v.light_id != ~0u) {
            _swap_back(scene.light_accel().remove_mesh(scene, v.light_id), mesh_lights.light_data);
            v.light_id = ~0u;
        }
    }
}

void Lights::update_point_light(
    CommandList &cmdlist,
    uint light_index,
    float3 center,
    float radius,
    float3 emission,
    bool visible) {
    auto &scene = SceneManager::instance();
    auto &data = point_lights.light_data[light_index];
    auto local_to_world = translation(center) * scaling(radius);
    if (data.tlas_id == ~0u) {
        if (visible) {
            data.tlas_id = scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                point_mesh,
                luisa::span{&data.mat_code, 1},
                translation(center) * scaling(radius),
                0xffu,
                true,
                data.light_id,
                LightAccel::t_PointLight);
        }
    } else {
        if (visible) {
            scene.accel_manager().set_mesh_instance(
                cmdlist,
                scene.buffer_uploader(),
                data.tlas_id,
                local_to_world,
                0xffu,
                true);
        } else {
            scene.accel_manager().remove_mesh_instance(
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                data.tlas_id);
            data.tlas_id = ~0u;
        }
    }
    /////////// Make new emission material instance
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    /////////// Update emission material
    scene.mat_manager().set_mat_instance(
        data.mat_code,
        scene.buffer_uploader(),
        luisa::span{reinterpret_cast<std::byte const *>(&mat_inst), sizeof(mat_inst)});
    LightAccel::PointLight point_light;
    point_light.sphere = make_float4(center, radius);
    point_light.radiance = LightAccel::PackFloat3(emission);
    point_light.mis_weight = visible ? 1 : -1;
    /////////// Update light accel
    scene.light_accel().update(
        scene,
        data.light_id,
        point_light);
}

void Lights::update_disk_light(
    CommandList &cmdlist,
    uint light_index,
    float3 center,
    float radius,
    float3 emission,
    float3 forward_dir,
    bool visible) {
    auto &scene = SceneManager::instance();
    auto &data = disk_lights.light_data[light_index];
    float4x4 local_to_world;
    float sign = copysign(1.0f, forward_dir.z);
    const float a = -(1.f / (sign + forward_dir.z));
    const float b = forward_dir.x * forward_dir.y * a;
    float3 tangent = float3(1.0f + sign * forward_dir.x * forward_dir.x * a, sign * b, -sign * forward_dir.x);
    float3 bitangent = float3(b, sign + forward_dir.y * forward_dir.y * a, -forward_dir.y);
    local_to_world.cols[0] = make_float4(tangent, 0);
    local_to_world.cols[1] = make_float4(bitangent, 0);
    local_to_world.cols[2] = make_float4(forward_dir, 0);
    local_to_world.cols[3] = make_float4(center, 1);
    local_to_world = local_to_world * scaling(radius);
    /////////// Update TLAS instance
    if (data.tlas_id == ~0u) {
        if (visible) {
            data.tlas_id = scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                disk_mesh,
                luisa::span{&data.mat_code, 1},
                local_to_world,
                0xffu,
                true,
                data.light_id,
                LightAccel::t_DiskLight// Disk light
            );
        }
    } else {
        if (visible) {
            scene.accel_manager().set_mesh_instance(
                cmdlist,
                scene.buffer_uploader(),
                data.tlas_id,
                local_to_world,
                0xffu,
                true);
        } else {
            scene.accel_manager().remove_mesh_instance(
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                data.tlas_id);
            data.tlas_id = ~0u;
        }
    }
    LightAccel::DiskLight disk_light;
    disk_light.forward_dir = LightAccel::PackFloat3(forward_dir);
    disk_light.radiance = LightAccel::PackFloat3(emission);
    disk_light.position = LightAccel::PackFloat3(center);
    disk_light.area = radius * radius * luisa::pi;
    disk_light.mis_weight = visible ? 1 : -1;
    ///////////// Make light emission material
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};
    mat_inst.tex.index = -1;
    scene.mat_manager().set_mat_instance(
        data.mat_code,
        scene.buffer_uploader(),
        luisa::span{reinterpret_cast<std::byte const *>(&mat_inst), sizeof(mat_inst)});
    /////////// Update light accel
    scene.light_accel().update(
        scene,
        data.light_id,
        disk_light);
}

void Lights::update_area_light(
    CommandList &cmdlist,
    uint light_index,
    float4x4 local_to_world,
    float3 emission,
    Image<float> const *emission_img,
    Sampler const *sampler,
    bool visible) {
    auto &scene = SceneManager::instance();
    auto &data = area_lights.light_data[light_index];
    /////////// Update TLAS instance
    if (data.tlas_id == ~0u) {
        if (visible) {
            data.tlas_id = scene.accel_manager().emplace_mesh_instance(
                cmdlist, scene.host_upload_buffer(),
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                scene.dispose_queue(),
                quad_mesh,
                luisa::span{&data.mat_code, 1},
                local_to_world,
                0xffu,
                true,
                data.light_id,
                LightAccel::t_AreaLight// Area light
            );
        }
    } else {
        if (visible) {
            scene.accel_manager().set_mesh_instance(
                cmdlist,
                scene.buffer_uploader(),
                data.tlas_id,
                local_to_world,
                0xffu,
                true);
        } else {
            scene.accel_manager().remove_mesh_instance(
                scene.buffer_allocator(),
                scene.buffer_uploader(),
                data.tlas_id);
            data.tlas_id = ~0u;
        }
    }
    /////////// Make new emission material instance
    LightAccel::AreaLight area_light;
    rbc::material::Unlit mat_inst{.color{emission.x, emission.y, emission.z}};

    if (emission_img) {
        area_light.emission_tex_id = scene.bindless_manager().enqueue_image(*emission_img, sampler ? *sampler : Sampler::anisotropic_edge());
    } else {
        area_light.emission_tex_id = ~0u;
    }
    mat_inst.tex.index = area_light.emission_tex_id;
    /////////// Update emission material
    scene.mat_manager().set_mat_instance(
        data.mat_code,
        scene.buffer_uploader(),
        luisa::span{reinterpret_cast<std::byte const *>(&mat_inst), sizeof(mat_inst)});
    float2 area{
        distance((local_to_world * float4(-0.5f, 0, 0, 1)).xyz(), (local_to_world * float4(0.5f, 0, 0, 1)).xyz()),
        distance((local_to_world * float4(0, -0.5f, 0, 1)).xyz(), (local_to_world * float4(0, 0.5f, 0, 1)).xyz())};
    area_light.area = area.x * area.y;
    area_light.radiance = LightAccel::PackFloat3(emission);
    area_light.transform = local_to_world;
    area_light.mis_weight = visible ? 1 : -1;

    /////////// Update light accel
    scene.light_accel().update(
        scene,
        data.light_id,
        area_light);
}

template<typename T>
void Lights::_swap_back(LightAccel::SwapBackCmd const &cmd, luisa::vector<T> &light_data) {
    if (cmd.transformed_user_id == -1u || cmd.new_light_index == -1u) return;
    light_data[cmd.transformed_user_id].light_id = cmd.new_light_index;
}

void Lights::remove_disk_light(uint light_index) {
    auto &scene = SceneManager::instance();
    auto &data = disk_lights.light_data[light_index];
    if (data.tlas_id != ~0u)
        scene.accel_manager().remove_mesh_instance(
            scene.buffer_allocator(),
            scene.buffer_uploader(),
            data.tlas_id);
    _swap_back(scene.light_accel().remove_disk(scene, data.light_id), disk_lights.light_data);
    scene.mat_manager().discard_mat_instance(data.mat_code);
    disk_lights.removed_list.emplace_back(light_index);
}

void Lights::remove_point_light(uint light_index) {
    auto &scene = SceneManager::instance();
    auto &data = point_lights.light_data[light_index];
    if (data.tlas_id != ~0u)
        scene.accel_manager().remove_mesh_instance(
            scene.buffer_allocator(),
            scene.buffer_uploader(),
            data.tlas_id);
    _swap_back(scene.light_accel().remove_point(scene, data.light_id), point_lights.light_data);
    scene.mat_manager().discard_mat_instance(data.mat_code);
    point_lights.removed_list.emplace_back(light_index);
}

void Lights::remove_area_light(uint light_index) {
    auto &scene = SceneManager::instance();
    auto &data = area_lights.light_data[light_index];
    if (data.tlas_id != ~0u)
        scene.accel_manager().remove_mesh_instance(
            scene.buffer_allocator(),
            scene.buffer_uploader(),
            data.tlas_id);
    _swap_back(scene.light_accel().remove_area(scene, data.light_id), area_lights.light_data);
    scene.mat_manager().discard_mat_instance(data.mat_code);
    area_lights.removed_list.emplace_back(light_index);
}

void Lights::remove_spot_light(uint light_index) {
    auto &scene = SceneManager::instance();
    auto &data = spot_lights.light_data[light_index];
    if (data.tlas_id != ~0u)
        scene.accel_manager().remove_mesh_instance(
            scene.buffer_allocator(),
            scene.buffer_uploader(),
            data.tlas_id);
    _swap_back(scene.light_accel().remove_spot(scene, data.light_id), spot_lights.light_data);
    scene.mat_manager().discard_mat_instance(data.mat_code);
    spot_lights.removed_list.emplace_back(light_index);
}

void Lights::remove_mesh_light(uint light_index) {
    auto &scene = SceneManager::instance();
    auto &data = mesh_lights.light_data[light_index];
    scene.dispose_after_sync(std::move(data.blas_buffer));
    data.device_mesh.reset();
    if (data.tlas_id != ~0u)
        scene.accel_manager().remove_mesh_instance(
            scene.buffer_allocator(),
            scene.buffer_uploader(),
            data.tlas_id);
    if (data.blas_heap_idx != uint(-1))
        scene.bindless_allocator().deallocate_buffer(data.blas_heap_idx);

    if (data.light_id != ~0u) {
        _swap_back(scene.light_accel().remove_mesh(scene, data.light_id), mesh_lights.light_data);
        data.light_id = ~0u;
    }
    mesh_lights.removed_list.emplace_back(light_index);
}

void Lights::dispose() {
    auto &scene = SceneManager::instance();
    if (quad_mesh)
        scene.mesh_manager().emplace_unload_mesh_cmd(quad_mesh);
    if (disk_mesh)
        scene.mesh_manager().emplace_unload_mesh_cmd(disk_mesh);
    if (point_mesh)
        scene.mesh_manager().emplace_unload_mesh_cmd(point_mesh);
    if (SceneManager::instance_ptr() != nullptr)
        scene.remove_before_render_event("_lights_tick");
}
Lights *Lights::instance() {
    return light_detail::_inst;
}
}// namespace rbc