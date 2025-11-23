#include <rbc_graphics/light_accel.h>
#include <rbc_graphics/scene_manager.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/light_type.h>
namespace rbc
{
float3 LightAccel::UnpackFloat3(std::array<float, 3> const& arr)
{
    return float3(arr[0], arr[1], arr[2]);
}
std::array<float, 3> LightAccel::PackFloat3(float3 const& arr)
{
    return std::array<float, 3>{ arr[0], arr[1], arr[2] };
}
namespace detail
{

float LightLuminance(float3 rad)
{
    return dot(rad, float3(0.2126729, 0.7151522, 0.0721750));
}
float LightLuminance(std::array<float, 3> const& rad)
{
    return LightLuminance(float3(rad[0], rad[1], rad[2]));
}
} // namespace detail
LightAccel::LightAccel(Device& device)
    : _device(device)
    , point_lights(luisa::to_underlying(LightType::Sphere))
    , spot_lights(luisa::to_underlying(LightType::Spot))
    , area_lights(luisa::to_underlying(LightType::Area))
    , mesh_lights(luisa::to_underlying(LightType::Blas))
    , disk_lights(luisa::to_underlying(LightType::Disk))
{
    // #ifdef ENABLE_TEMPORAL_DI
    // #endif
}

template <typename T>
uint LightAccel::Light<T>::emplace(
    LightAccel& self,
    DisposeQueue& disp_queue,
    CommandList& cmdlist,
    BufferUploader& uploader,
    T const& data,
    uint user_id
)
{
    uint index = host_data.size();
    auto accel_id = self._inst_ids.size();
    self._inst_ids.emplace_back(InstIndex{ light_type, uint(index) });
    auto& node = self._input_nodes.emplace_back(data.leaf_node());
    node.write_index(light_type, index);
    host_data.emplace_back(data, accel_id, user_id);
    if (host_data.size() >= LightAccel::t_MaxLightCount) [[unlikely]]
    {
        LUISA_ERROR("Light count out of range {}", t_MaxLightCount);
    }
    size_t buffer_size = device_data ? device_data.size() : 0;
    if (host_data.capacity() > buffer_size) [[unlikely]]
    {
        auto new_buffer = self._device.create_buffer<T>(std::max(host_data.capacity(), 65536u / sizeof(T)));
        if (device_data)
        {
            uploader.swap_buffer(device_data, new_buffer);
            cmdlist << new_buffer.view(0, index).copy_from(device_data.view(0, index));
            disp_queue.dispose_after_queue(std::move(device_data));
        }
        device_data = std::move(new_buffer);
    }
    uploader.emplace_copy_cmd(device_data.view(index, 1), &data);
    return index;
}

uint* LightAccel::get_accel_id(uint light_type, uint index)
{
    uint* ptr{};
    switch (static_cast<LightType>(light_type))
    {
    case LightType::Sphere:
        ptr = &point_lights.host_data[index].accel_id;
        break;
    case LightType::Spot:
        ptr = &spot_lights.host_data[index].accel_id;
        break;
    case LightType::Area:
        ptr = &area_lights.host_data[index].accel_id;
        break;
    case LightType::Disk:
        ptr = &disk_lights.host_data[index].accel_id;
        break;
    case LightType::Blas:
        ptr = &mesh_lights.host_data[index].accel_id;
        break;
    default:
        LUISA_ERROR_WITH_LOCATION("Invalid light type id.");
        break;
    }
    return ptr;
}

template <typename T>
auto LightAccel::Light<T>::remove(
    LightAccel& self,
    BufferUploader& uploader,
    uint index
) -> SwapBackCmd
{
    auto& curr_data = host_data[index];
    auto accel_id = curr_data.accel_id;
    // remove self in light-list
    SwapBackCmd cmd{
        -1u,
        -1u
    };
    if (index != host_data.size() - 1)
    {
        cmd.transformed_user_id = host_data.back().user_id;
        cmd.new_light_index = index;
        curr_data = host_data.back();
        self._inst_ids[curr_data.accel_id].light_index = index;
        self._input_nodes[curr_data.accel_id].write_index(light_type, index);
        uploader.emplace_copy_cmd(device_data.view(index, 1), &curr_data.data);
    }
    host_data.pop_back();
    // remove self in accel
    self._erase_accel_inst(accel_id);
    return cmd;
}
template <typename T>
void LightAccel::Light<T>::update(
    LightAccel& self,
    BufferUploader& uploader,
    uint index,
    T const& t
)
{
    host_data[index].data = t;
    uploader.emplace_copy_cmd(device_data.view(index, 1), &t);
}

void LightAccel::_erase_accel_inst(uint accel_id)
{
    if (accel_id == (_input_nodes.size() - 1))
    {
        _input_nodes.pop_back();
        _inst_ids.pop_back();
        return;
    }
    auto& inst_id = _inst_ids.back();
    auto vec_ptr = get_accel_id(inst_id.light_type, inst_id.light_index);
    (*vec_ptr) = accel_id;
    _inst_ids[accel_id] = inst_id;
    _inst_ids.pop_back();
    _input_nodes[accel_id] = _input_nodes.back();
    _input_nodes.pop_back();
}
void LightAccel::reserve_tlas()
{
    capacity = _input_nodes.size() * 2;
    _tlas_data.reserve(capacity);
}
void LightAccel::build_tlas()
{
    if (!_dirty) return;
    BVH::build(_tlas_data, _input_nodes);
    capacity = _tlas_data.size();
}
void LightAccel::update_tlas(CommandList& cmdlist, DisposeQueue& disp_queue)
{
    if (!_dirty || capacity == 0) return;
    if (_tlas_buffer && _tlas_buffer.size() < capacity)
    {
        disp_queue.dispose_after_queue(std::move(_tlas_buffer));
    }
    if (!_tlas_buffer) [[unlikely]]
    {
        _tlas_buffer =
            _device.create_buffer<BVH::PackedNode>(
                std::max(capacity, 65536 / sizeof(BVH::PackedNode))
            );
    }

    cmdlist << _tlas_buffer.view(0, capacity).copy_from(_tlas_data.data());
}
void LightAccel::mark_light_dirty(
    uint light_type,
    uint light_index
)
{
    // reserved
}
LightAccel::~LightAccel() {}
BVH::InputNode LightAccel::PointLight::leaf_node() const
{
    BVH::InputNode r;
    r.bounding.min = sphere.xyz() - make_float3(sphere.w);
    r.bounding.max = sphere.xyz() + make_float3(sphere.w);
    r.bary_center_and_weight = make_float4(
        sphere.xyz(),
        rbc::detail::LightLuminance(radiance) * pi * sphere.w * sphere.w
    );
    r.cone = make_float4(0, 0, 1, pi);
    return r;
}
BVH::InputNode LightAccel::SpotLight::leaf_node() const
{
    BVH::InputNode r;
    r.bounding.min = sphere.xyz() - make_float3(sphere.w);
    r.bounding.max = sphere.xyz() + make_float3(sphere.w);
    r.bary_center_and_weight = make_float4(
        sphere.xyz(),
        rbc::detail::LightLuminance(radiance) * pi * sphere.w * sphere.w * (angle_radian / pi)
    );
    r.cone = make_float4(UnpackFloat3(forward_dir), angle_radian);
    return r;
}
BVH::InputNode LightAccel::AreaLight::leaf_node() const
{
    BVH::InputNode r;
    float3 light_dir = normalize((transform * float4(0, 0, 1, 0)).xyz());
    float3 left_down = (transform * float4(-0.5f, -0.5f, 0.0f, 1.0f)).xyz();
    float3 right_down = (transform * float4(0.5f, -0.5f, 0.0f, 1.0f)).xyz();
    float3 left_up = (transform * float4(-0.5f, 0.5f, 0.0f, 1.0f)).xyz();
    float3 right_up = (transform * float4(0.5f, 0.5f, 0.0f, 1.0f)).xyz();
    r.bounding = BVH::Bounding{
        .min = min(left_down.xyz(), min(right_down.xyz(), min(left_up.xyz(), right_up.xyz()))),
        .max = max(left_down.xyz(), max(right_down.xyz(), max(left_up.xyz(), right_up.xyz()))),
    };
    r.bary_center_and_weight = make_float4(
        (transform * float4(0, 0, 0, 1)).xyz(),
        rbc::detail::LightLuminance(radiance) * area
    );
    r.cone = make_float4(light_dir, luisa::pi_over_four);
    return r;
}
BVH::InputNode LightAccel::DiskLight::leaf_node() const
{
    BVH::InputNode r;
    float radius = sqrt(area / luisa::pi);
    float3 norm(forward_dir[0], forward_dir[1], forward_dir[2]);
    float3 e = radius * sqrt(1.0f - norm * norm);
    float3 pos(position[0], position[1], position[2]);
    r.bounding = BVH::Bounding{
        .min = pos - e,
        .max = pos + e
    };
    r.bary_center_and_weight = make_float4(
        pos,
        rbc::detail::LightLuminance(radiance) * area
    );
    r.cone = make_float4(norm, luisa::pi_over_four);
    return r;
}
BVH::InputNode LightAccel::MeshLight::leaf_node() const
{
    auto bounding_min = UnpackFloat3(this->bounding_min);
    auto bounding_max = UnpackFloat3(this->bounding_max);
    BVH::InputNode n;
    n.bounding.min = bounding_min.xyz();
    n.bounding.max = bounding_max.xyz();
    n.bary_center_and_weight = make_float4(lerp(bounding_min.xyz(), bounding_max.xyz(), float3(0.5f)), lum);
    n.cone = make_float4(0, 0, 1, pi);
    return n;
}

uint LightAccel::emplace(
    CommandList& cmdlist,
    SceneManager& scene_manager,
    AreaLight const& area_light,
    uint user_id
)
{
    _dirty = true;
    auto idx = area_lights.emplace(
        *this,
        scene_manager.dispose_queue(),
        cmdlist,
        scene_manager.buffer_uploader(),
        area_light, user_id
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_AreaLight, idx);
    // #endif
    return idx;
}
uint LightAccel::emplace(
    CommandList& cmdlist,
    SceneManager& scene_manager,
    DiskLight const& disk_light,
    uint user_id
)
{
    _dirty = true;
    auto idx = disk_lights.emplace(
        *this,
        scene_manager.dispose_queue(),
        cmdlist,
        scene_manager.buffer_uploader(),
        disk_light, user_id
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_DiskLight, idx);
    // #endif
    return idx;
}
uint LightAccel::emplace(
    CommandList& cmdlist,
    SceneManager& scene_manager,
    PointLight const& point_light,
    uint user_id
)
{
    _dirty = true;
    auto idx = point_lights.emplace(
        *this,
        scene_manager.dispose_queue(),
        cmdlist,
        scene_manager.buffer_uploader(),
        point_light, user_id
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_PointLight, idx);
    // #endif
    return idx;
}
uint LightAccel::emplace(
    CommandList& cmdlist,
    SceneManager& scene_manager,
    SpotLight const& spot_light,
    uint user_id
)
{
    _dirty = true;
    auto idx = spot_lights.emplace(
        *this,
        scene_manager.dispose_queue(),
        cmdlist,
        scene_manager.buffer_uploader(),
        spot_light, user_id
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_SpotLight, idx);
    // #endif
    return idx;
}
uint LightAccel::emplace(
    CommandList& cmdlist,
    SceneManager& scene_manager,
    MeshLight const& mesh_light,
    uint user_id
)
{
    auto& transform = mesh_light.transform;
    float3 bounding_min{
        mesh_light.bounding_min[0],
        mesh_light.bounding_min[1],
        mesh_light.bounding_min[2]
    };
    float3 bounding_max{
        mesh_light.bounding_max[0],
        mesh_light.bounding_max[1],
        mesh_light.bounding_max[2]
    };
    float3 min_v{ std::numeric_limits<float>::max() };
    float3 max_v{ std::numeric_limits<float>::min() };
    float3 v = (transform * make_float4(bounding_min, 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_max, 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_min.x, bounding_max.yz(), 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_max.x, bounding_min.y, bounding_max.z, 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_max.xy(), bounding_min.z, 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_max.x, bounding_min.yz(), 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_min.x, bounding_max.y, bounding_min.z, 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    v = (transform * make_float4(bounding_min.xy(), bounding_max.z, 1.0f)).xyz();
    min_v = min(v, min_v);
    max_v = max(v, max_v);
    MeshLight new_mesh_light{
        .transform = transform,
        .bounding_min = { min_v.x, min_v.y, min_v.z },
        .blas_heap_idx = mesh_light.blas_heap_idx,
        .bounding_max = { max_v.x, max_v.y, max_v.z },
        .instance_user_id = mesh_light.instance_user_id,
        .lum = mesh_light.lum,
        .mis_weight = mesh_light.mis_weight
    };

    _dirty = true;
    auto idx = mesh_lights.emplace(
        *this,
        scene_manager.dispose_queue(),
        cmdlist,
        scene_manager.buffer_uploader(),
        new_mesh_light, user_id
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_MeshLight, idx);
    // #endif
    return idx;
}
void LightAccel::update(
    SceneManager& scene_manager,
    uint index,
    AreaLight const& area_light
)
{
    _dirty = true;
    area_lights.update(
        *this,
        scene_manager.buffer_uploader(),
        index,
        area_light
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_AreaLight, index);
    // #endif
}
void LightAccel::update(
    SceneManager& scene_manager,
    uint index,
    DiskLight const& disk_light
)
{
    _dirty = true;
    disk_lights.update(
        *this,
        scene_manager.buffer_uploader(),
        index,
        disk_light
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_DiskLight, index);
    // #endif
}
void LightAccel::update(
    SceneManager& scene_manager,
    uint index,
    PointLight const& point_light
)
{
    _dirty = true;
    point_lights.update(
        *this,
        scene_manager.buffer_uploader(),
        index,
        point_light
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_PointLight, index);
    // #endif
}
void LightAccel::update(
    SceneManager& scene_manager,
    uint index,
    SpotLight const& spot_light
)
{
    _dirty = true;
    spot_lights.update(
        *this,
        scene_manager.buffer_uploader(),
        index,
        spot_light
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_SpotLight, index);
    // #endif
}
void LightAccel::update(
    SceneManager& scene_manager,
    uint index,
    MeshLight const& mesh_light
)
{
    _dirty = true;
    mesh_lights.update(
        *this,
        scene_manager.buffer_uploader(),
        index,
        mesh_light
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_MeshLight, index);
    // #endif
}
auto LightAccel::remove_area(
    SceneManager& scene_manager,
    uint index
) -> SwapBackCmd
{
    _dirty = true;
    auto cmd = area_lights.remove(
        *this,
        scene_manager.buffer_uploader(),
        index
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_AreaLight, area_lights.host_data.size());
    mark_light_dirty(t_AreaLight, index);
    return cmd;
    // #endif
}
auto LightAccel::remove_disk(
    SceneManager& scene_manager,
    uint index
) -> SwapBackCmd
{
    _dirty = true;
    auto cmd = disk_lights.remove(
        *this,
        scene_manager.buffer_uploader(),
        index
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_DiskLight, disk_lights.host_data.size());
    mark_light_dirty(t_DiskLight, index);
    return cmd;
    // #endif
}
auto LightAccel::remove_spot(
    SceneManager& scene_manager,
    uint index
) -> SwapBackCmd
{
    _dirty = true;
    auto cmd = spot_lights.remove(
        *this,
        scene_manager.buffer_uploader(),
        index
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_SpotLight, spot_lights.host_data.size());
    mark_light_dirty(t_SpotLight, index);
    // #endif
    return cmd;
}
auto LightAccel::remove_point(
    SceneManager& scene_manager,
    uint index
) -> SwapBackCmd
{
    _dirty = true;
    auto cmd = point_lights.remove(
        *this,
        scene_manager.buffer_uploader(),
        index
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_PointLight, point_lights.host_data.size());
    mark_light_dirty(t_PointLight, index);
    // #endif
    return cmd;
}
auto LightAccel::remove_mesh(
    SceneManager& scene_manager,
    uint index
) -> SwapBackCmd
{
    _dirty = true;
    auto cmd = mesh_lights.remove(
        *this,
        scene_manager.buffer_uploader(),
        index
    );
    // #ifdef ENABLE_TEMPORAL_DI
    mark_light_dirty(t_MeshLight, mesh_lights.host_data.size());
    mark_light_dirty(t_MeshLight, index);
    // #endif
    return cmd;
}
void LightAccel::generate_sphere_mesh(
    luisa::vector<float3>& vertices,
    luisa::vector<uint>& triangles,
    uint order
)
{
    const float f = (1.0 + std::pow(5.0, 0.5)) / 2.0;
    uint64_t T = 1;
    for (auto i : vstd::range(order))
    {
        T *= 4;
    }
    auto init_vertices = {
        -1.0f, f, 0.0f, 1.0f, f, 0.0f, -1.0f, -f, 0.0f, 1.0f, -f, 0.0f,
        0.0f, -1.0f, f, 0.0f, 1.0f, f, 0.0f, -1.0f, -f, 0.0f, 1.0f, -f,
        f, 0.0f, -1.0f, f, 0.0f, 1.0f, -f, 0.0f, -1.0f, -f, 0.0f, 1.0f
    };
    {
        auto v = init_vertices.begin();
        vertices.resize_uninitialized(std::max(init_vertices.size() / 3, (10 * T + 2)));
        for (auto i : vstd::range(init_vertices.size() / 3))
        {
            vertices[i] = float3(v[0], v[1], v[2]);
            v += 3;
        }
    }
    triangles = std::initializer_list<uint>{
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
        11, 10, 2, 5, 11, 4, 1, 5, 9, 7, 1, 8, 10, 7, 6,
        3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
        9, 8, 1, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7
    };
    vstd::unordered_map<uint, uint> midCache;
    uint v = 12;
    auto addMidPoint = [&](uint a, uint b) {
        uint key = ((a + b) * (a + b + 1u) / 2u) + std::min(a, b);
        auto iter = midCache.find(key);
        if (iter != midCache.end())
        {
            auto i = iter->second;
            midCache.erase(iter);
            return i;
        }
        midCache.try_emplace(key, v);
        vertices[v] = (vertices[a] + vertices[b]) / 2.0f;
        return v++;
    };
    auto trianglesPrev = std::move(triangles);
    for (uint i = 0; i < order; i++)
    {
        triangles.resize_uninitialized(trianglesPrev.size() * 4);
        for (uint k = 0; k < trianglesPrev.size(); k += 3)
        {
            const auto v1 = trianglesPrev[k + 0];
            const auto v2 = trianglesPrev[k + 1];
            const auto v3 = trianglesPrev[k + 2];
            const auto a = addMidPoint(v1, v2);
            const auto b = addMidPoint(v2, v3);
            const auto c = addMidPoint(v3, v1);
            auto t = k * 4;
            triangles[t++] = v1;
            triangles[t++] = a;
            triangles[t++] = c;
            triangles[t++] = v2;
            triangles[t++] = b;
            triangles[t++] = a;
            triangles[t++] = v3;
            triangles[t++] = c;
            triangles[t++] = b;
            triangles[t++] = a;
            triangles[t++] = b;
            triangles[t++] = c;
        }
        trianglesPrev = std::move(triangles);
    }
    triangles = std::move(trianglesPrev);
    for (auto& i : vertices)
    {
        i = normalize(i);
    }
}
void LightAccel::load_shader(luisa::fiber::counter& counter)
{
}

} // namespace rbc