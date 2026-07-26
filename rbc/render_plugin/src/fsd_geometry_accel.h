#pragma once
#include <fsd/constants.hpp>
#include <rbc_graphics/accel_manager.h>

#include <limits>

namespace rbc {

struct SceneManager;

struct FsdGeometryAccel {
private:
    using BuildTriangleAabbsShader = Shader1D<
        BindlessArray,
        Buffer<AABB>,
        uint,
        uint,
        float3>;
    using ClearEdgeHeadsShader = Shader1D<Buffer<uint>>;
    using BuildEdgeLinksShader = Shader1D<
        BindlessArray,
        Buffer<uint>,
        Buffer<uint>,
        uint,
        uint,
        uint,
        uint,
        uint>;
    using ResolveEdgeAdjacencyShader = Shader1D<
        BindlessArray,
        Buffer<uint>,
        Buffer<uint>,
        Buffer<uint>,
        uint,
        uint,
        uint,
        uint,
        uint>;

    struct InstanceSignature {
        MeshManager::MeshData *mesh{};
        float4x4 transform{};
        uint triangle_count{};
        uint user_id{};
        uint8_t visibility_mask{};
    };

    struct MeshProxy {
        Buffer<AABB> aabbs;
        ProceduralPrimitive primitive;
    };

    SceneManager &_scene;
    BuildTriangleAabbsShader const *_build_triangle_aabbs{};
    ClearEdgeHeadsShader const *_clear_edge_heads{};
    BuildEdgeLinksShader const *_build_edge_links{};
    ResolveEdgeAdjacencyShader const *_resolve_edge_adjacency{};
    Accel _accel;
    Buffer<uint> _edge_adjacency;
    Buffer<uint> _instance_adjacency_offsets;
    vector<MeshProxy> _mesh_proxies;
    vector<InstanceSignature> _instances;

    void _dispose_resources();

public:
    explicit FsdGeometryAccel(SceneManager &scene) noexcept : _scene(scene) {}
    FsdGeometryAccel(FsdGeometryAccel const &) = delete;
    FsdGeometryAccel(FsdGeometryAccel &&) = delete;
    FsdGeometryAccel &operator=(FsdGeometryAccel const &) = delete;
    FsdGeometryAccel &operator=(FsdGeometryAccel &&) = delete;

    [[nodiscard]] bool sync(CommandList &cmdlist);
    void clear();

    [[nodiscard]] Accel const &accel() const noexcept { return _accel; }
    [[nodiscard]] Buffer<uint> const &edge_adjacency() const noexcept {
        return _edge_adjacency;
    }
    [[nodiscard]] Buffer<uint> const &instance_adjacency_offsets() const noexcept {
        return _instance_adjacency_offsets;
    }
    [[nodiscard]] bool empty() const noexcept { return !_accel; }
};

struct FsdResources {
private:
    static constexpr uint32_t invalid_bindless_index =
        std::numeric_limits<uint32_t>::max();

    SceneManager &_scene;
    FsdGeometryAccel _geometry;
    Buffer<float> _inverse_cdf_lut;
    fsd::BufferIndices _buffer_indices{
        .edge_adjacency = invalid_bindless_index,
        .instance_adjacency_offsets = invalid_bindless_index,
        .inverse_cdf_lut = invalid_bindless_index};

    void _ensure_lut(CommandList &cmdlist);
    void _bind_buffers(CommandList &cmdlist);
    [[nodiscard]] bool _release_bindless_buffers() noexcept;

public:
    explicit FsdResources(SceneManager &scene) noexcept;
    ~FsdResources();

    FsdResources(FsdResources const &) = delete;
    FsdResources(FsdResources &&) = delete;
    FsdResources &operator=(FsdResources const &) = delete;
    FsdResources &operator=(FsdResources &&) = delete;

    void sync(CommandList &cmdlist);
    void clear(CommandList &cmdlist);

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] Accel const &accel() const noexcept {
        return _geometry.accel();
    }
    [[nodiscard]] fsd::BufferIndices const &buffer_indices() const noexcept {
        return _buffer_indices;
    }
};

}// namespace rbc
