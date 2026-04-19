#pragma once
#include <rbc_world/resource_base.h>
#include <rbc_world/resources/procedural_resource.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_buffer.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/rtx/procedural_primitive.h>
#include <luisa/runtime/rtx/aabb.h>

namespace rbc {
struct DisposeQueue;
#include <geometry/procedural_types.hpp>
}// namespace rbc

namespace rbc::world {

/// Resource class for Height Map data
struct RBC_RUNTIME_API HeightMapResource final : ProceduralResource {
    DECLARE_WORLD_OBJECT_FRIEND(HeightMapResource)
    using BaseType = ResourceBaseImpl<HeightMapResource>;
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Resource;

private:
    mutable rbc::shared_atomic_mutex _async_mtx;

    HeightMapResource();
    ~HeightMapResource();
    // meta infos
    uint2 _resolution;
    // Resources
    RC<DeviceImage> _height_img;
    RC<DeviceBuffer> _aabb_and_height_bounding;// Layout: <AABB, block_size()> + <float, block_size()>

    // Procedural primitive data for ray tracing integration
    rbc::geometry::HeightMap _height_map_surface;
    uint32_t _procedural_instance_id{~0u};///< Instance ID in AccelManager
    bool _procedural_prim_dirty{true};    ///< Flag to indicate if procedural primitive needs rebuild

    /// Compute AABBs for height map blocks
    void _compute_aabbs(luisa::compute::CommandList &cmdlist) const;

public:
    /// Check if resource is empty
    [[nodiscard]] bool empty() const;
    auto height_img() const { return _height_img.get(); }
    uint2 block_size() const {
        return _resolution / 32u;
    }

    /// Create an empty height map with given resolution
    void create_empty(uint2 resolution);
    /// Serialize metadata for persistence
    void serialize_meta(ObjSerialize const &ser) const override;

    /// Deserialize metadata from storage
    void deserialize_meta(ObjDeSerialize const &ser) override;

    /// Async loading coroutine
    rbc::coroutine _async_load() override;

    /// Get the AABB buffer for the height map blocks
    [[nodiscard]] luisa::compute::BufferView<luisa::compute::AABB> aabb_buffer() const {
        auto s = block_size();
        return _aabb_and_height_bounding->buffer().view(0, s.x * s.y * sizeof(AABB) / sizeof(uint)).as<AABB>();
    }
    [[nodiscard]] luisa::compute::BufferView<float> height_bounding_buffer() const {
        auto s = block_size();
        return _aabb_and_height_bounding->buffer().view(s.x * s.y * sizeof(AABB) / sizeof(uint), s.x * s.y * sizeof(float) / sizeof(uint)).as<float>();
    }

    /// Get the HeightMap surface data for shader access
    [[nodiscard]] rbc::geometry::HeightMap const &height_map_surface() const { return _height_map_surface; }
    [[nodiscard]] rbc::geometry::HeightMap &height_map_surface() { return _height_map_surface; }

    /// ProceduralResource interface
    [[nodiscard]] bool is_procedural_dirty() const override { return _procedural_prim_dirty; }
    [[nodiscard]] uint32_t procedural_instance_id() const override { return _procedural_instance_id; }
    void build_procedural_primitive(
        luisa::compute::CommandList &cmdlist,
        luisa::compute::ProceduralPrimitive &procedural_prim,
        DisposeQueue &disp_queue) override;
    [[nodiscard]] uint emplace_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu) override;
    void set_procedural_instance(
        luisa::float4x4 const &transform,
        uint8_t visibility_mask = 0xffu,
        bool opaque = false) override;
    void remove_procedural_instance() override;

protected:
    bool _install() override;
    bool unsafe_save_to_path() const override;

public:
    [[nodiscard]] BaseObjectType base_type() const override;
    [[nodiscard]] MD5 type_id() const override;
    [[nodiscard]] const char *type_name() const override;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::HeightMapResource)
