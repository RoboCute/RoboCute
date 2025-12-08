#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/mesh_light_accel.h>
#include <luisa/runtime/image.h>
#include <rbc_core/rc.h>
#include <luisa/vstl/functional.h>
namespace rbc {
struct TexStreamManager;
struct DeviceMesh;
struct RBC_RUNTIME_API Lights : public SceneManagerEvent {
    struct LightData {
        uint tlas_id;
        MatCode mat_code;
        uint light_id;
    };
    enum struct MeshLightLoadState : uint32_t {
        Unloaded,
        Loaded,
        Disposed,
    };
    struct MeshLightData {
        Buffer<BVH::PackedNode> blas_buffer;
        RC<DeviceMesh> device_mesh;
        vstd::optional<luisa::fiber::future<MeshLightAccel::HostResult>> host_result;
        luisa::shared_ptr<std::atomic<MeshLightLoadState>> _load_flag;
        uint blas_heap_idx;
        uint tlas_id;
        uint light_id;
        uint tlas_light_id;
    };
    template<typename T>
    struct LightList {
        luisa::vector<size_t> removed_list;
        luisa::vector<T> light_data;
    };
    LightList<LightData> area_lights;
    LightList<LightData> disk_lights;
    LightList<LightData> point_lights;
    LightList<LightData> spot_lights;
    LightList<MeshLightData> mesh_lights;

    MeshLightAccel mesh_light_accel;
    MeshManager::MeshData *point_mesh{};
    MeshManager::MeshData *quad_mesh{};
    MeshManager::MeshData *disk_mesh{};
    Lights();
    // return: light index
    uint add_area_light(
        CommandList &cmdlist,
        float4x4 local_to_world,
        float3 emission,
        Image<float> const *emission_img = nullptr,
        Sampler const *sampler = nullptr,
        bool visible = true);

    uint add_disk_light(
        CommandList &cmdlist,
        float3 center,
        float radius,
        float3 emission,
        float3 forward_dir,
        bool visible = true);

    uint add_point_light(
        CommandList &cmdlist,
        float3 center,
        float radius,
        float3 emission,
        bool visible = true);

    uint add_spot_light(
        CommandList &cmdlist,
        float3 center,
        float radius,
        float3 emission,
        float3 forward_dir,
        float angle,      // radian
        float small_angle,// radian
        float angle_atten_pow,
        bool visible = true);

    uint add_mesh_light_sync(
        CommandList &cmdlist,
        RC<DeviceMesh> const &device_mesh,
        float4x4 local_to_world,
        luisa::span<MatCode const> material_codes);

    void update_mesh_light_sync(
        CommandList &cmdlist,
        uint light_index,
        float4x4 local_to_world,
        luisa::span<MatCode const> material_codes,
        RC<DeviceMesh> const *new_mesh = nullptr);

    void update_area_light(
        CommandList &cmdlist,
        uint light_index,
        float4x4 local_to_world,
        float3 emission,
        Image<float> const *emission_img = nullptr,
        Sampler const *sampler = nullptr,
        bool visible = true);

    void update_disk_light(
        CommandList &cmdlist,
        uint light_index,
        float3 center,
        float radius,
        float3 emission,
        float3 forward_dir,
        bool visible = true);

    void update_spot_light(
        CommandList &cmdlist,
        uint light_index,
        float3 center,
        float radius,
        float3 emission,
        float3 forward_dir,
        float angle,      // radian
        float small_angle,// radian
        float angle_atten_pow,
        bool visible = true);

    void update_point_light(
        CommandList &cmdlist,
        uint light_index,
        float3 center,
        float radius,
        float3 emission,
        bool visible = true);

    void remove_point_light(uint light_index);
    void remove_area_light(uint light_index);
    void remove_spot_light(uint light_index);
    void remove_mesh_light(uint light_index);
    void remove_disk_light(uint light_index);

    void dispose();
    void scene_manager_tick() override;
    ~Lights();
    static Lights *instance();

private:
    template<typename T>
    static void _swap_back(LightAccel::SwapBackCmd const &cmd, luisa::vector<T> &light_data);
    luisa::spin_mutex _before_render_mtx;
    vstd::vector<vstd::function<bool()>> _before_render_funcs;
    void add_tick(vstd::function<bool()> &&func);
};
}// namespace rbc