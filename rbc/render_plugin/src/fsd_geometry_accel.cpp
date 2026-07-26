#include "fsd_geometry_accel.h"
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_core/base.h>
#include <fsd/lut.hpp>
#include <spectrum/spectrum_args.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace rbc {
namespace fsd_geometry_accel_detail {

static const AccelOption kAccelOption{
    .hint = AccelOption::UsageHint::FAST_TRACE,
    .allow_compaction = false,
    .allow_update = false};

struct FsdTableHeader {
    uint32_t magic;
    uint32_t format_version;
    uint32_t generator_version;
    uint32_t resolution;
    uint32_t float_count;
    uint32_t offsets[4];
};

constexpr auto kFsdTableFilename = "fsd_tables.bytes";
constexpr uint32_t kFsdTableMagic = 0x4c445346u;
constexpr uint32_t kFsdTableFormatVersion = 2u;
constexpr uint32_t kFsdTableGeneratorVersion = 2u;
constexpr float kMaxSearchRadius = fsd::query_radius_from_wavelength_nm(
    spectrum::wavelength_max);

[[nodiscard]] static MeshManager::MeshData *candidate_mesh(
    AccelManager::AccelElement const &element) noexcept {
    if (!element.opaque ||
        !element.mesh_data.is_type_of<MeshManager::MeshData *>()) {
        return nullptr;
    }
    auto mesh = element.mesh_data.force_get<MeshManager::MeshData *>();
    return mesh != nullptr &&
                   !mesh->is_vertex_instance &&
                   mesh->triangle_size != 0u
               ? mesh
               : nullptr;
}

[[nodiscard]] static bool local_expansion(
    float4x4 const &local_to_world,
    float world_radius,
    float3 &result) noexcept {
    for (auto column : vstd::range(4u)) {
        for (auto row : vstd::range(4u)) {
            if (!std::isfinite(local_to_world[column][row])) {
                return false;
            }
        }
    }
    auto const determinant =
        local_to_world[0][0] *
            (local_to_world[1][1] * local_to_world[2][2] -
             local_to_world[2][1] * local_to_world[1][2]) -
        local_to_world[1][0] *
            (local_to_world[0][1] * local_to_world[2][2] -
             local_to_world[2][1] * local_to_world[0][2]) +
        local_to_world[2][0] *
            (local_to_world[0][1] * local_to_world[1][2] -
             local_to_world[1][1] * local_to_world[0][2]);
    if (!std::isfinite(determinant) || determinant == 0.0f) {
        return false;
    }
    auto const world_to_local = inverse(local_to_world);
    auto row_length = [&](uint row) noexcept {
        auto const x = world_to_local[0][row];
        auto const y = world_to_local[1][row];
        auto const z = world_to_local[2][row];
        return std::sqrt(x * x + y * y + z * z);
    };
    result = world_radius * make_float3(
        row_length(0),
        row_length(1),
        row_length(2));
    return std::isfinite(result.x) &&
           std::isfinite(result.y) &&
           std::isfinite(result.z);
}

}// namespace fsd_geometry_accel_detail

void FsdGeometryAccel::_dispose_resources() {
    auto &disp_queue = _scene.dispose_queue();
    if (_accel) {
        disp_queue.dispose_after_queue(std::move(_accel));
    }
    for (auto &proxy : _mesh_proxies) {
        if (proxy.primitive) {
            disp_queue.dispose_after_queue(std::move(proxy.primitive));
        }
        if (proxy.aabbs) {
            disp_queue.dispose_after_queue(std::move(proxy.aabbs));
        }
    }
    _mesh_proxies.clear();
    auto dispose_buffer = [&](auto &buffer) {
        if (buffer) {
            disp_queue.dispose_after_queue(std::move(buffer));
        }
    };
    dispose_buffer(_edge_adjacency);
    dispose_buffer(_instance_adjacency_offsets);
}

void FsdGeometryAccel::clear() {
    _dispose_resources();
    _instances.clear();
}

bool FsdGeometryAccel::sync(CommandList &cmdlist) {
    using namespace fsd_geometry_accel_detail;
    auto const &buffer_heap = _scene.buffer_heap();
    auto const &scene_accel = _scene.accel_manager();
    auto &device = _scene.device();
    auto &disp_queue = _scene.dispose_queue();

    struct Candidate {
        InstanceSignature signature;
        float3 local_expansion;
    };
    vector<Candidate> candidates;
    for (auto inst_id : vstd::range(scene_accel.mesh_instance_size())) {
        auto element_ptr = scene_accel.try_get_accel_element(inst_id);
        if (element_ptr == nullptr) continue;
        auto const &element = *element_ptr;
        auto mesh = candidate_mesh(element);
        if (mesh == nullptr) {
            continue;
        }
        float3 expansion;
        if (!local_expansion(
                element.transform,
                kMaxSearchRadius,
                expansion)) {
            continue;
        }
        auto &candidate = candidates.emplace_back();
        candidate.signature = InstanceSignature{
            .mesh = mesh,
            .transform = element.transform,
            .triangle_count = mesh->triangle_size,
            .user_id = element.user_id,
            .visibility_mask = element.visibility_mask};
        candidate.local_expansion = expansion;
    }

    if (candidates.empty()) {
        auto const changed = !empty();
        clear();
        return changed;
    }

    auto unchanged = !_scene.accel_dirty() &&
                     candidates.size() == _instances.size();
    if (unchanged) {
        auto same_signature = [](InstanceSignature const &lhs,
                                 InstanceSignature const &rhs) noexcept {
            return lhs.mesh == rhs.mesh &&
                   lhs.triangle_count == rhs.triangle_count &&
                   lhs.user_id == rhs.user_id &&
                   lhs.visibility_mask == rhs.visibility_mask &&
                   std::memcmp(&lhs.transform, &rhs.transform, sizeof(float4x4)) == 0;
        };
        for (auto i : vstd::range(candidates.size())) {
            if (!same_signature(candidates[i].signature, _instances[i])) {
                unchanged = false;
                break;
            }
        }
    }
    if (unchanged) {
        return false;
    }

    if (_build_triangle_aabbs == nullptr) {
        ShaderManager::instance()->load(
            "fsd/build_triangle_aabbs.bin",
            _build_triangle_aabbs);
        LUISA_ASSERT(
            _build_triangle_aabbs != nullptr,
            "Failed to load FSD triangle AABB builder shader.");
    }
    if (_clear_edge_heads == nullptr) {
        ShaderManager::instance()->load(
            "fsd/clear_edge_heads.bin",
            _clear_edge_heads);
        ShaderManager::instance()->load(
            "fsd/build_edge_links.bin",
            _build_edge_links);
        ShaderManager::instance()->load(
            "fsd/resolve_edge_adjacency.bin",
            _resolve_edge_adjacency);
        LUISA_ASSERT(
            _clear_edge_heads != nullptr &&
                _build_edge_links != nullptr &&
                _resolve_edge_adjacency != nullptr,
            "Failed to load FSD edge adjacency shaders.");
    }

    struct UniqueMesh {
        MeshManager::MeshData *mesh;
        float3 local_expansion;
    };
    vector<UniqueMesh> unique_meshes;
    vstd::unordered_map<MeshManager::MeshData *, uint> mesh_indices;
    mesh_indices.reserve(candidates.size());
    for (auto const &candidate : candidates) {
        auto [iter, inserted] = mesh_indices.try_emplace(
            candidate.signature.mesh,
            static_cast<uint>(unique_meshes.size()));
        if (inserted) {
            unique_meshes.emplace_back(UniqueMesh{
                .mesh = candidate.signature.mesh,
                .local_expansion = candidate.local_expansion});
            continue;
        }
        auto &expansion = unique_meshes[iter->second].local_expansion;
        expansion = make_float3(
            std::max(expansion.x, candidate.local_expansion.x),
            std::max(expansion.y, candidate.local_expansion.y),
            std::max(expansion.z, candidate.local_expansion.z));
    }

    vector<MeshProxy> new_proxies;
    new_proxies.reserve(unique_meshes.size());
    vector<uint> mesh_adjacency_offsets;
    mesh_adjacency_offsets.reserve(unique_meshes.size());
    uint64_t total_edge_count_64 = 0u;
    uint64_t total_head_count_64 = 0u;
    vector<uint> mesh_head_counts;
    mesh_head_counts.reserve(unique_meshes.size());
    for (auto const &unique : unique_meshes) {
        auto const edge_count_64 =
            static_cast<uint64_t>(unique.mesh->triangle_size) * 3u;
        auto const minimum_head_count = std::max<uint64_t>(2u, edge_count_64 * 2u);
        auto const head_count_64 = std::bit_ceil(minimum_head_count);
        LUISA_ASSERT(
            total_edge_count_64 + edge_count_64 <=
                    std::numeric_limits<uint>::max() &&
                total_head_count_64 + head_count_64 <=
                    std::numeric_limits<uint>::max(),
            "FSD edge adjacency exceeds 32-bit indexing.");
        total_edge_count_64 += edge_count_64;
        total_head_count_64 += head_count_64;
        mesh_head_counts.emplace_back(static_cast<uint>(head_count_64));
    }
    auto const total_edge_count = static_cast<uint>(total_edge_count_64);
    auto const total_head_count = static_cast<uint>(total_head_count_64);
    auto new_edge_adjacency = device.create_buffer<uint>(total_edge_count);
    auto new_edge_heads = device.create_buffer<uint>(total_head_count);
    auto new_edge_next = device.create_buffer<uint>(total_edge_count);
    cmdlist << (*_clear_edge_heads)(new_edge_heads).dispatch(total_head_count);

    uint edge_offset = 0u;
    uint head_offset = 0u;
    uint mesh_index = 0u;
    for (auto const &unique : unique_meshes) {
        auto &proxy = new_proxies.emplace_back();
        mesh_adjacency_offsets.emplace_back(edge_offset);
        proxy.aabbs = device.create_buffer<AABB>(
            unique.mesh->triangle_size);
        proxy.primitive = device.create_procedural_primitive(
            proxy.aabbs,
            kAccelOption);
        cmdlist << (*_build_triangle_aabbs)(
                       buffer_heap,
                       proxy.aabbs,
                       unique.mesh->meta.heap_idx,
                       unique.mesh->meta.tri_byte_offset,
                       unique.local_expansion)
                       .dispatch(unique.mesh->triangle_size);
        cmdlist << proxy.primitive.build();

        auto const edge_count = unique.mesh->triangle_size * 3u;
        auto const head_count = mesh_head_counts[mesh_index++];
        cmdlist << (*_build_edge_links)(
                       buffer_heap,
                       new_edge_heads,
                       new_edge_next,
                       head_offset,
                       edge_offset,
                       unique.mesh->meta.heap_idx,
                       unique.mesh->meta.tri_byte_offset,
                       head_count - 1u)
                       .dispatch(edge_count);
        cmdlist << (*_resolve_edge_adjacency)(
                       buffer_heap,
                       new_edge_heads,
                       new_edge_next,
                       new_edge_adjacency,
                       head_offset,
                       edge_offset,
                       unique.mesh->meta.heap_idx,
                       unique.mesh->meta.tri_byte_offset,
                       head_count - 1u)
                       .dispatch(edge_count);
        edge_offset += edge_count;
        head_offset += head_count;
    }

    auto new_accel = device.create_accel(kAccelOption);
    vector<uint> instance_adjacency_offsets;
    instance_adjacency_offsets.reserve(candidates.size());
    for (auto const &candidate : candidates) {
        auto const proxy_index = mesh_indices.find(candidate.signature.mesh)->second;
        new_accel.emplace_back(
            new_proxies[proxy_index].primitive,
            candidate.signature.transform,
            candidate.signature.visibility_mask,
            candidate.signature.user_id);
        instance_adjacency_offsets.emplace_back(
            mesh_adjacency_offsets[proxy_index]);
    }
    cmdlist << new_accel.build();
    auto new_instance_adjacency_offsets =
        device.create_buffer<uint>(instance_adjacency_offsets.size());
    cmdlist << new_instance_adjacency_offsets.copy_from(
        luisa::span{instance_adjacency_offsets});
    disp_queue.dispose_after_queue(
        std::move(instance_adjacency_offsets));
    disp_queue.dispose_after_queue(std::move(new_edge_heads));
    disp_queue.dispose_after_queue(std::move(new_edge_next));

    _dispose_resources();
    _accel = std::move(new_accel);
    _edge_adjacency = std::move(new_edge_adjacency);
    _instance_adjacency_offsets =
        std::move(new_instance_adjacency_offsets);
    _mesh_proxies = std::move(new_proxies);
    _instances.clear();
    _instances.reserve(candidates.size());
    for (auto const &candidate : candidates) {
        _instances.emplace_back(candidate.signature);
    }
    return true;
}

FsdResources::FsdResources(SceneManager &scene) noexcept
    : _scene{scene}, _geometry{scene} {}

FsdResources::~FsdResources() {
    LUISA_ASSERT(
        _geometry.empty() &&
            !_inverse_cdf_lut &&
            _buffer_indices.edge_adjacency == invalid_bindless_index &&
            _buffer_indices.instance_adjacency_offsets ==
                invalid_bindless_index &&
            _buffer_indices.inverse_cdf_lut == invalid_bindless_index,
        "FSD resources must be cleared while their scene is still alive.");
}

void FsdResources::_ensure_lut(CommandList &cmdlist) {
    using namespace fsd_geometry_accel_detail;
    if (_inverse_cdf_lut) return;

    auto const runtime_dir =
        RenderDevice::instance().lc_ctx().runtime_directory();
    BinaryFileStream file_stream{
        luisa::to_string(runtime_dir / kFsdTableFilename)};
    luisa::vector<std::byte> bytes;
    bytes.resize_uninitialized(file_stream.length());
    file_stream.read(luisa::span{bytes});
    LUISA_ASSERT(
        bytes.size_bytes() >= sizeof(FsdTableHeader),
        "FSD inverse-CDF table is truncated.");

    FsdTableHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    LUISA_ASSERT(
        header.magic == kFsdTableMagic &&
            header.format_version == kFsdTableFormatVersion &&
            header.generator_version == kFsdTableGeneratorVersion &&
            header.resolution == fsd::inverse_cdf_resolution &&
            header.float_count == fsd::inverse_cdf_float_count &&
            header.offsets[0] == fsd::alpha1_theta_lut_offset &&
            header.offsets[1] == fsd::alpha1_radial_lut_offset &&
            header.offsets[2] == fsd::alpha2_theta_lut_offset &&
            header.offsets[3] == fsd::alpha2_radial_lut_offset &&
            bytes.size_bytes() == sizeof(FsdTableHeader) +
                sizeof(float) * header.float_count,
        "FSD inverse-CDF table header is invalid.");

    _inverse_cdf_lut = _scene.device().create_buffer<float>(
        header.float_count);
    auto table_data = luisa::span{
        reinterpret_cast<float const *>(
            bytes.data() + sizeof(FsdTableHeader)),
        static_cast<size_t>(header.float_count)};
    cmdlist << _inverse_cdf_lut.copy_from(table_data);
    _scene.dispose_after_commit(std::move(bytes));
}

bool FsdResources::_release_bindless_buffers() noexcept {
    bool changed = false;
    auto release = [&](uint32_t &index) {
        if (index == invalid_bindless_index) return;
        _scene.bindless_allocator().deallocate_buffer(index);
        index = invalid_bindless_index;
        changed = true;
    };
    release(_buffer_indices.edge_adjacency);
    release(_buffer_indices.instance_adjacency_offsets);
    release(_buffer_indices.inverse_cdf_lut);
    return changed;
}

void FsdResources::_bind_buffers(CommandList &cmdlist) {
    static_cast<void>(_release_bindless_buffers());
    auto &allocator = _scene.bindless_allocator();
    _buffer_indices.edge_adjacency = allocator.allocate_buffer(
        _geometry.edge_adjacency());
    _buffer_indices.instance_adjacency_offsets =
        allocator.allocate_buffer(
            _geometry.instance_adjacency_offsets());
    _buffer_indices.inverse_cdf_lut = allocator.allocate_buffer(
        _inverse_cdf_lut);
    allocator.commit(cmdlist);
}

void FsdResources::sync(CommandList &cmdlist) {
    auto const geometry_changed = _geometry.sync(cmdlist);
    if (_geometry.empty()) {
        clear(cmdlist);
        return;
    }
    _ensure_lut(cmdlist);
    if (geometry_changed || !active()) {
        _bind_buffers(cmdlist);
    }
}

void FsdResources::clear(CommandList &cmdlist) {
    auto const bindings_changed = _release_bindless_buffers();
    _geometry.clear();
    if (_inverse_cdf_lut) {
        _scene.dispose_queue().dispose_after_queue(
            std::move(_inverse_cdf_lut));
    }
    if (bindings_changed) {
        _scene.bindless_allocator().commit(cmdlist);
    }
}

bool FsdResources::active() const noexcept {
    return !_geometry.empty() &&
           static_cast<bool>(_inverse_cdf_lut) &&
           _buffer_indices.edge_adjacency != invalid_bindless_index &&
           _buffer_indices.instance_adjacency_offsets !=
               invalid_bindless_index &&
           _buffer_indices.inverse_cdf_lut != invalid_bindless_index;
}

}// namespace rbc
