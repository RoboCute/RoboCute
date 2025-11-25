#pragma once
#include <rbc_config.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/bvh.h>
#include <rbc_graphics/buffer_uploader.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/image.h>
#include <luisa/core/fiber.h>
#include <luisa/core/stl/vector.h>
namespace rbc
{
struct SceneManager;
struct RBC_RUNTIME_API LightAccel {
public:
    static float3 UnpackFloat3(std::array<float, 3> const& arr);
    static std::array<float, 3> PackFloat3(float3 const& arr);
    // -1 for not swaping
    struct SwapBackCmd {
        uint transformed_user_id;
        uint new_light_index;
    };
    struct PointLight {
        float4 sphere;
        std::array<float, 3> radiance;
        float mis_weight{ 1 };
        BVH::InputNode leaf_node() const;
    };

    struct SpotLight {
        std::array<float, 3> radiance;
        float angle_radian; // half
        float4 sphere;
        std::array<float, 3> forward_dir;
        float small_angle_radian; // half
        float angle_atten_power;
        float mis_weight{ 1 };
        BVH::InputNode leaf_node() const;
    };
    struct AreaLight {
        float4x4 transform;
        std::array<float, 3> radiance;
        float area;
        uint emission_tex_id;
        float mis_weight{ 1 };
        BVH::InputNode leaf_node() const;
    };
    struct DiskLight {
        std::array<float, 3> forward_dir;
        float area;
        std::array<float, 3> radiance;
		std::array<float, 3> position;
        float mis_weight{ 1 };
        BVH::InputNode leaf_node() const;
    };
    struct MeshLight {
        float4x4 transform;
        std::array<float, 3> bounding_min;
        uint blas_heap_idx;
        std::array<float, 3> bounding_max;
        uint instance_user_id;
        float lum;
        float mis_weight{ 1 };
        BVH::InputNode leaf_node() const;
    };
    static constexpr uint32_t t_PointLight = 0;
    static constexpr uint32_t t_SpotLight = 1;
    static constexpr uint32_t t_AreaLight = 2;
    static constexpr uint32_t t_MeshLight = 3;
    static constexpr uint32_t t_DiskLight = 4;
    static constexpr uint32_t t_LightCount = t_DiskLight + 1;
    static constexpr uint32_t t_MaxLightCount = 16384u;

private:
    template <typename T>
    struct Light
    {
        private:
        friend struct LightAccel;
        uint light_type : 3;
        struct DataPack {
            T data;
            uint accel_id;
            uint user_id;
        };
        vector<DataPack> host_data;
        Buffer<T> device_data;

    public:
        Light(uint type)
            : light_type(type)
        {
        }
        uint emplace(
            LightAccel& self,
            DisposeQueue& disp_queue,
            CommandList& cmdlist,
            BufferUploader& uploader,
            T const& data,
            uint user_id
        );
        SwapBackCmd remove(
            LightAccel& self,
            BufferUploader& uploader,
            uint index
        );
        void update(
            LightAccel& self,
            BufferUploader& uploader,
            uint index,
            T const& t
        );
        [[nodiscard]] auto const& buffer() const
        {
            return device_data;
        }
        // get next light id
        uint _get_next_id() const
        {
            return host_data.size();
        }
    };
    struct InstIndex {
        uint light_type : 3;
        uint light_index : 29;
    };
    

    uint* get_accel_id(uint light_type, uint index);
    vector<BVH::InputNode> _input_nodes;
    Device& _device;
    vector<InstIndex> _inst_ids;
    ///////////// Accel
    vector<BVH::PackedNode> _tlas_data;
    Buffer<BVH::PackedNode> _tlas_buffer;
    Light<PointLight> point_lights;
    Light<SpotLight> spot_lights;
    Light<AreaLight> area_lights;
    Light<MeshLight> mesh_lights;
    Light<DiskLight> disk_lights;
    void _erase_accel_inst(uint accel_id);
    size_t capacity{ 0 };
    bool _dirty = false;

public:
    [[nodiscard]] uint _get_next_pointlight_index() const { return point_lights._get_next_id(); }
    [[nodiscard]] uint _get_next_arealight_index() const { return area_lights._get_next_id(); }
    [[nodiscard]] uint _get_next_disklight_index() const { return disk_lights._get_next_id(); }
    [[nodiscard]] uint _get_next_spotlight_index() const { return spot_lights._get_next_id(); }
    [[nodiscard]] uint _get_next_meshlight_index() const { return mesh_lights._get_next_id(); }
    // #endif
    [[nodiscard]] auto light_count() const
    {
        return point_lights.host_data.size() +
               area_lights.host_data.size() +
               disk_lights.host_data.size() +
               spot_lights.host_data.size() +
               mesh_lights.host_data.size();
    }
    [[nodiscard]] auto const& area_light_buffer() const { return area_lights.buffer(); }
    [[nodiscard]] auto const& disk_light_buffer() const { return disk_lights.buffer(); }
    [[nodiscard]] auto const& spot_light_buffer() const { return spot_lights.buffer(); }
    [[nodiscard]] auto const& point_light_buffer() const { return point_lights.buffer(); }
    [[nodiscard]] auto const& mesh_light_buffer() const { return mesh_lights.buffer(); }
    [[nodiscard]] auto tlas_data() const { return span<BVH::PackedNode const>{ _tlas_data }; }
    [[nodiscard]] auto const& tlas_buffer() const { return _tlas_buffer; }
    LightAccel(Device& device);
    void load_shader(luisa::fiber::counter& counter);
    void reserve_tlas();
    void build_tlas();
    void update_tlas(CommandList& cmdlist, DisposeQueue& disp_queue);
    static void generate_sphere_mesh(
        luisa::vector<float3>& vertices,
        luisa::vector<uint>& triangles,
        uint order = 3
    );
    void mark_light_dirty(
        uint light_type,
        uint light_index
    );
    uint emplace(
        CommandList& cmdlist,
        SceneManager& scene_manager,
        AreaLight const& area_light,
        uint user_id = -1
    );
    uint emplace(
        CommandList& cmdlist,
        SceneManager& scene_manager,
        DiskLight const& disk_light,
        uint user_id = -1
    );
    uint emplace(
        CommandList& cmdlist,
        SceneManager& scene_manager,
        PointLight const& point_light,
        uint user_id = -1
    );
    uint emplace(
        CommandList& cmdlist,
        SceneManager& scene_manager,
        SpotLight const& spot_light,
        uint user_id = -1
    );
    uint emplace(
        CommandList& cmdlist,
        SceneManager& scene_manager,
        MeshLight const& mesh_light,
        uint user_id = -1
    );
    void update(
        SceneManager& scene_manager,
        uint index,
        AreaLight const& area_light
    );
    void update(
        SceneManager& scene_manager,
        uint index,
        DiskLight const& disk_light
    );
    void update(
        SceneManager& scene_manager,
        uint index,
        PointLight const& point_light
    );
    void update(
        SceneManager& scene_manager,
        uint index,
        SpotLight const& spot_light
    );
    void update(
        SceneManager& scene_manager,
        uint index,
        MeshLight const& mesh_light
    );
    SwapBackCmd remove_area(
        SceneManager& scene_manager,
        uint index
    );
    SwapBackCmd remove_disk(
        SceneManager& scene_manager,
        uint index
    );
    SwapBackCmd remove_spot(
        SceneManager& scene_manager,
        uint index
    );
    SwapBackCmd remove_point(
        SceneManager& scene_manager,
        uint index
    );
    SwapBackCmd remove_mesh(
        SceneManager& scene_manager,
        uint index
    );

    ~LightAccel();
};
} // namespace rbc