#include <rbc_graphics/mesh_manager.h>
#include <luisa/runtime/device.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc {
namespace mesh_mng_detail {
static float uint_unpack_to_float(uint val) {
    uint uvalue = val >> uint(31u) == 0 ? (~val) : (val & uint(~(1u << 31u)));
    return reinterpret_cast<float &>(uvalue);
}
}// namespace mesh_mng_detail
MeshManager::MeshManager(Device &device)
    : device(device), pool(256, false) {
}
void MeshManager::load_shader(luisa::fiber::counter &counter) {
    ShaderManager::instance()->async_load(counter, "geometry/set_submesh.bin", set_submesh);
    ShaderManager::instance()->async_load(counter, "geometry/compute_bound.bin", compute_bound);
}

void MeshManager::emplace_build_mesh_cmd(
    MeshData *mesh_data,
    AccelOption const &option) {
    std::lock_guard lck{_mtx};
    _build_cmds.emplace_back(mesh_data, option);
}
void MeshManager::emplace_unload_mesh_cmd(
    MeshData *mesh_data) {
    std::lock_guard lck{_mtx};
    _unload_cmds.emplace_back(mesh_data);
}
void MeshManager::MeshData::create_blas(Device &device, CommandList &cmdlist, AccelOption const &option) {
    auto &&data_buffer = pack.data;
    if (!pack.mesh) {
        auto vertex_count = meta.vertex_count;
        uint tri_offset = meta.tri_byte_offset / sizeof(uint);
        BufferView<float3> vb = data_buffer.view(0, vertex_count * sizeof(float3) / sizeof(uint)).as<float3>();
        BufferView<Triangle> ib = data_buffer.view(tri_offset, data_buffer.size() - tri_offset).as<Triangle>();
        pack.mesh = device.create_mesh(vb, ib, option);
    }
}
void MeshManager::MeshData::build_mesh(Device &device, CommandList &cmdlist, AccelOption const &option) {
    create_blas(device, cmdlist, option);
    cmdlist << pack.mesh.build();
}

void MeshManager::execute_build_cmds(CommandList &cmdlist, BindlessAllocator &bdls_alloc, HostBufferManager &temp_buffer) {
    luisa::vector<std::pair<MeshData *, AccelOption>> build_cmds;
    {
        std::lock_guard lck{_mtx};
        build_cmds = std::move(_build_cmds);
    }
    if (!build_cmds.empty()) {
        for (auto &i : build_cmds) {
            auto mesh_data = i.first;
            auto &&option = i.second;
            if (mesh_data->pack.mesh) [[unlikely]] {
                LUISA_ERROR("Mesh BLAS already initialized.");
            }
            mesh_data->build_mesh(device, cmdlist, option);
            // if (!mesh_data->submesh_offset.empty() && !mesh_data->pack.submesh_indices)
            // _create_submesh_buffer(cmdlist, bdls_alloc, temp_buffer, mesh_data);
        }
    }
}
void MeshManager::on_frame_end(
    CommandList &cmdlist,
    BindlessAllocator &bdls_alloc) {
    luisa::vector<MeshData *> unload_cmds;
    {
        std::lock_guard lck{_mtx};
        unload_cmds = std::move(_unload_cmds);
    }
    if (!unload_cmds.empty()) {
        for (auto &mesh_data : unload_cmds) {
            mesh_data->submesh_offset.clear();
            if (!mesh_data->is_vertex_instance && mesh_data->meta.submesh_heap_idx != std::numeric_limits<uint>::max()) {
                bdls_alloc.deallocate_buffer(mesh_data->meta.submesh_heap_idx);
            }
            bdls_alloc.deallocate_buffer(mesh_data->meta.heap_idx);
            if (mesh_data->bbox_requests) {
                mesh_data->bbox_requests->mesh_data = nullptr;
            }
        }
        cmdlist.add_callback([this, vec = std::move(unload_cmds)]() {
            for (auto &i : vec) {
                pool.destroy_lock(_pool_mtx, std::launder(reinterpret_cast<MeshDataPack *>(i)));
            }
        });
    }
}

auto MeshManager::load_mesh(
    BindlessAllocator &bdls_alloc,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    Buffer<uint> &&data_buffer,
    vstd::optional<AccelOption> option,
    uint vertex_count,
    bool normal,
    bool tangent,
    uint uv_count,
    vstd::vector<uint> &&submesh_offset,
    bool calculate_bounding) -> MeshData * {
    uint stride = sizeof(float3);
    uint mask = 0;
    if (normal) {
        stride += sizeof(float3);
        mask |= MeshMeta::normal_mask;
    }
    if (tangent) {
        stride += sizeof(float4);
        mask |= MeshMeta::tangent_mask;
    }
    LUISA_ASSERT(uv_count < 29, "UV reach maximum count.");
    stride += uv_count * sizeof(float2);
    for (auto i : vstd::range(uv_count)) {
        mask |= MeshMeta::uv_mask << i;
    }

    auto m = reinterpret_cast<MeshData *>(pool.create_lock(_pool_mtx));
    m->meta = MeshMeta{
        .tri_byte_offset = stride * vertex_count,
        .vertex_count = vertex_count,
        .ele_mask = mask};
    m->is_vertex_instance = false;
    uint tri_offset = m->meta.tri_byte_offset / sizeof(uint);
    if (tri_offset >= data_buffer.size()) [[unlikely]] {
        LUISA_ERROR("Illegal mesh vertex format.");
    }
    BufferView<Triangle> ib = data_buffer.view(tri_offset, data_buffer.size() - tri_offset).as<Triangle>();
    if (option) {
        BufferView<float3> vb = data_buffer.view(0, vertex_count * sizeof(float3) / sizeof(uint)).as<float3>();
        m->pack.mesh = device.create_mesh(
            vb,
            ib,
            *option);
        cmdlist << m->pack.mesh.build();
    }
    if (calculate_bounding) {
        m->bbox_requests = new BBoxRequest();
        m->bbox_requests->mesh_data = m;
        m->bbox_requests->bounding_box.push_back_uninitialized(
            std::max<size_t>(1, submesh_offset.size()));
        cmdlist.add_callback([this, m] {
            _bounding_mtx.lock();
            _bounding_requests.emplace_back(m->bbox_requests);
            _bounding_mtx.unlock();
        });
    }
    m->pack.data = std::move(data_buffer);
    m->triangle_size = ib.size();
    m->meta.heap_idx = bdls_alloc.allocate_buffer(m->pack.data);
    if (!submesh_offset.empty()) {
        LUISA_ASSERT(submesh_offset.size() > 1, "Should have more than one submesh-offset.");
        LUISA_ASSERT(submesh_offset[0] == 0, "First element must be 0.");
        m->submesh_offset = std::move(submesh_offset);
        _create_submesh_buffer(cmdlist, bdls_alloc, temp_buffer, m);
    } else {
        m->meta.submesh_heap_idx = std::numeric_limits<uint>::max();
    }
    return m;
}

void MeshManager::_create_submesh_buffer(
    CommandList &cmdlist,
    BindlessAllocator &bdls_alloc,
    HostBufferManager &temp_buffer,
    MeshData *m) {
    auto &&submesh_indices = m->pack.submesh_indices;
    LUISA_DEBUG_ASSERT(!submesh_indices);
    auto &submesh_offset = m->submesh_offset;
    submesh_indices = device.create_buffer<uint16_t>(m->triangle_size);
    auto offset_buffer = temp_buffer.allocate_upload_buffer(submesh_offset.size_bytes(), 16);
    memcpy(offset_buffer.mapped_ptr(), submesh_offset.data(), submesh_offset.size_bytes());
    vstd::vector<uint3> dispatch_list;
    dispatch_list.push_back_uninitialized(submesh_offset.size());
    for (auto i : vstd::range(submesh_offset.size())) {
        uint disp_size;
        if (i == (submesh_offset.size() - 1)) {
            disp_size = submesh_indices.size();
        } else {
            disp_size = submesh_offset[i + 1];
        }
        if (submesh_offset[i] >= disp_size) [[unlikely]] {
            LUISA_ERROR("Illegal mesh index format. {} {}", submesh_offset[i], disp_size);
        }
        disp_size -= submesh_offset[i];
        dispatch_list[i] = uint3(disp_size, 1, 1);
    }
    cmdlist << (*set_submesh)(submesh_indices, offset_buffer.view).dispatch(dispatch_list);
    m->meta.submesh_heap_idx = bdls_alloc.allocate_buffer(submesh_indices);
}

auto MeshManager::make_transforming_instance(
    BindlessAllocator &bdls_alloc,
    CommandList &cmdlist,
    MeshData *mesh_data) -> MeshData * {
    auto result = reinterpret_cast<MeshData *>(pool.create_lock(_pool_mtx));
    result->pack.data = device.create_buffer<uint>(mesh_data->pack.data.size());
    result->meta = mesh_data->meta;
    result->meta.heap_idx = bdls_alloc.allocate_buffer(result->pack.data);
    result->triangle_size = mesh_data->triangle_size;
    result->is_vertex_instance = true;
    return result;
}

void MeshManager::execute_compute_bounding(
    CommandList &cmdlist,
    BindlessArray const &buffer_heap,
    HostBufferManager &temp_buffer,
    DisposeQueue &dsp_queue) {
    _bounding_mtx.lock();
    auto bounding_requests = std::move(_bounding_requests);
    _bounding_mtx.unlock();
    if (bounding_requests.empty()) return;
    luisa::vector<uint2> args;
    luisa::vector<uint3> dispatch_sizes;
    for (size_t i = 0; i < bounding_requests.size();) {
        auto &rq = bounding_requests[i];
        auto mesh = rq->mesh_data;
        if (!mesh) {
            rq = bounding_requests.back();
            continue;
        }
        ++i;
        for (auto submesh_idx : vstd::range(std::max<size_t>(1, mesh->submesh_offset.size()))) {
            uint triangle_offset = (submesh_idx < mesh->submesh_offset.size() ? mesh->submesh_offset[submesh_idx] : 0);
            args.emplace_back(
                mesh->meta.heap_idx,
                triangle_offset * 3 +
                    mesh->meta.tri_byte_offset / sizeof(uint));
            auto next_idx = submesh_idx + 1;
            auto triangles_size = (next_idx < mesh->submesh_offset.size() ? mesh->submesh_offset[next_idx] : mesh->triangle_size) - triangle_offset;
            dispatch_sizes.emplace_back(triangles_size * 3, 1, 1);
            // debug
            // luisa::vector<float3> host_vec(mesh->meta.vertex_count);
            // luisa::vector<uint> host_indices(triangles_size * 3);
            // cmdlist << mesh->pack.data.view(0, host_vec.size_bytes() / sizeof(uint)).copy_to(host_vec.data())
            //         << mesh->pack.data.view(args.back().y, host_indices.size()).copy_to(host_indices.data());
            // cmdlist.add_callback([rq, submesh_idx, host_vec = std::move(host_vec), host_indices = std::move(host_indices)] {
            //     float3 min_val{ 1e20f };
            //     float3 max_val{ -1e20f };
            //     auto& r = rq->bounding_box[submesh_idx];
            //     for (auto& i : host_indices)
            //     {
            //         min_val = min(host_vec[i], min_val);
            //         max_val = max(host_vec[i], max_val);
            //     }
            //     for (auto i : vstd::range(3))
            //     {
            //         r.packed_min[i] = min_val[i];
            //         r.packed_max[i] = max_val[i];
            //     }
            //     rq->finished = true;
            // });
        }
    }
    luisa::vector<AABB> results;
    results.push_back_uninitialized(args.size());
    if (_aabb_cache_buffer && _aabb_cache_buffer.size() < args.size()) {
        dsp_queue.dispose_after_queue(std::move(_aabb_cache_buffer));
    }
    if (!_aabb_cache_buffer) {
        auto aligned_size = 65536 / sizeof(AABB);
        auto target_size = (args.size() + aligned_size - 1) / aligned_size * aligned_size;
        _aabb_cache_buffer = device.create_buffer<AABB>(target_size);
    }
    auto arg_buffer = temp_buffer.allocate_upload_buffer<uint2>(args.size());
    std::memcpy(arg_buffer.mapped_ptr(), args.data(), args.size_bytes());
    auto result_view = _aabb_cache_buffer.view(0, results.size());
    cmdlist << (*compute_bound)(
                   buffer_heap,
                   arg_buffer.view,
                   result_view.as<uint>(),
                   true)
                   .dispatch(result_view.as<uint>().size())
            << (*compute_bound)(
                   buffer_heap,
                   arg_buffer.view,
                   result_view.as<uint>(),
                   false)
                   .dispatch(dispatch_sizes)
            << result_view.copy_to(results.data());
    cmdlist.add_callback([results = std::move(results), bounding_requests = std::move(bounding_requests)]() mutable {
        auto iter = results.begin();
        for (auto &i : bounding_requests) {
            uint sub_idx = 0;
            for (auto &sub : i->bounding_box) {
                sub = *iter;
                for (auto i : vstd::range(3)) {
                    sub.packed_min[i] = mesh_mng_detail::uint_unpack_to_float(reinterpret_cast<uint &>(sub.packed_min[i]));
                    sub.packed_max[i] = mesh_mng_detail::uint_unpack_to_float(reinterpret_cast<uint &>(sub.packed_max[i]));
                }
                sub_idx++;
                LUISA_DEBUG_ASSERT(iter != results.end());
                ++iter;
            }
            i->finished = true;
        }
        LUISA_DEBUG_ASSERT(iter == results.end());
    });
}

MeshManager::~MeshManager() {}
}// namespace rbc