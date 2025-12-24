#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_io/io_command_list.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_core/atomic.h>
#include <rbc_core/utils/binary_search.h>
namespace rbc {
DeviceMesh::DeviceMesh() {
}
DeviceMesh::~DeviceMesh() {
    if (_render_mesh_data) {
        auto inst = AssetsManager::instance();
        if (inst) {
            inst
                ->scene_mng()
                ->mesh_manager()
                .emplace_unload_mesh_cmd(_render_mesh_data);
        }
    }
}

template<typename LoadType>
void DeviceMesh::_async_load(
    LoadType &&load_type,
    uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool build_mesh, bool calculate_bound,
    uint32_t triangle_size,
    bool copy_to_host, uint64_t extra_data_size) {
    if ((extra_data_size & 3) > 0) [[unlikely]] {
        LUISA_ERROR("Extra size must be aligned as 4-byte.");
    }
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push(
        [vertex_count, build_mesh, calculate_bound, copy_to_host, extra_data_size, triangle_size, normal, tangent, uv_count, this_shared = RC{this}, submesh_triangle_offset = std::move(submesh_triangle_offset), load_type = std::move(load_type)](LoadTaskArgs const &args) mutable {
            auto ptr = static_cast<DeviceMesh *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            vstd::optional<AccelOption> opt;
            if (build_mesh)
                opt.create(AccelOption{.allow_compaction = false});
            /////////// Load as file
            uint64_t desired_size = get_mesh_size(vertex_count, normal, tangent, uv_count, triangle_size);
            if constexpr (std::is_same_v<LoadType, FileLoad>) {
                luisa::filesystem::path const &path = load_type.path;
                uint64_t file_offset = load_type.file_offset;
                auto file = args.io_cmdlist.retrieve(luisa::to_string(path));
                if (!file) [[unlikely]] {
                    LUISA_ERROR("DeviceMesh path {} not found.", luisa::to_string(path));
                }
                uint64_t size = (file.length() - file_offset) + sizeof(uint) - 1;
                if (size < desired_size + extra_data_size) [[unlikely]] {
                    LUISA_ERROR("Mesh file size {} less than required size {}", size, desired_size + extra_data_size);
                }

                auto data_buffer = inst->lc_device().create_buffer<uint>(desired_size / sizeof(uint));
                if (copy_to_host) {
                    *args.require_disk_io_sync = true;
                    auto &host_data = this_shared->_host_data;
                    host_data.clear();
                    host_data.push_back_uninitialized(desired_size + extra_data_size);
                    args.io_cmdlist << IOCommand{
                        file,
                        file_offset,
                        host_data};
                    args.mem_io_cmdlist << IOCommand{
                        IOCommand::SrcType{host_data.data()},
                        0,
                        IOBufferSubView(data_buffer)};
                } else {
                    args.io_cmdlist << IOCommand(
                        file,
                        file_offset,
                        IOBufferSubView(data_buffer));
                }

                ptr->_render_mesh_data = inst->scene_mng()->mesh_manager().load_mesh(
                    inst->scene_mng()->bindless_allocator(),
                    args.cmdlist,
                    *args.temp_buffer,
                    std::move(data_buffer),
                    opt,
                    vertex_count,
                    normal,
                    tangent,
                    uv_count,
                    std::move(submesh_triangle_offset),
                    calculate_bound);
            }
            /////////// Load as memory
            else if constexpr (std::is_same_v<LoadType, MemoryLoad>) {
                BinaryBlob const &data = load_type.blob;
                if (data.size() < desired_size + extra_data_size) [[unlikely]] {
                    LUISA_ERROR("Mesh memory size {} less than required size {}", data.size(), desired_size + extra_data_size);
                }
                auto data_buffer = inst->lc_device().create_buffer<uint>(desired_size / sizeof(uint));
                args.mem_io_cmdlist << IOCommand(
                    data.data(),
                    0,
                    IOBufferSubView(data_buffer));
                ptr->_render_mesh_data = inst->scene_mng()->mesh_manager().load_mesh(
                    inst->scene_mng()->bindless_allocator(),
                    args.cmdlist,
                    *args.temp_buffer,
                    std::move(data_buffer),
                    opt,
                    vertex_count,
                    normal,
                    tangent,
                    uv_count,
                    std::move(submesh_triangle_offset),
                    true);
            }
            /////////// Load as Buffer
            else if constexpr (std::is_same_v<LoadType, BufferLoad>) {
                Buffer<uint> &data_buffer = load_type.buffer;
                ptr->_render_mesh_data = inst->scene_mng()->mesh_manager().load_mesh(
                    inst->scene_mng()->bindless_allocator(),
                    args.cmdlist,
                    *args.temp_buffer,
                    std::move(data_buffer),
                    opt,
                    vertex_count,
                    normal,
                    tangent,
                    uv_count,
                    std::move(submesh_triangle_offset));
            }
        });
}

void DeviceMesh::async_load_from_buffer(
    Buffer<uint> &&data,
    uint vertex_count,
    uint32_t triangle_size,
    bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool calculate_bound) {
    _async_load(BufferLoad{std::move(data)}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), true, calculate_bound, triangle_size);
}

void DeviceMesh::async_load_from_memory(
    BinaryBlob &&data,
    uint vertex_count,
    uint32_t triangle_size,
    bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool build_mesh,
    bool calculate_bound,
    bool copy_to_host,
    uint64_t extra_data_size) {
    if (copy_to_host) {
        if (data.data()) {
            if (_host_data.size() != data.size()) {
                _host_data.clear();
                _host_data.push_back_uninitialized(data.size());
            }
            std::memcpy(_host_data.data(), data.data(), data.size());
        }
        data = BinaryBlob{
            _host_data.data(),
            _host_data.size(),
            {}};
    }
    _async_load(MemoryLoad{std::move(data)}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), build_mesh, calculate_bound, triangle_size, extra_data_size);
}

void DeviceMesh::async_load_from_file(
    luisa::filesystem::path const &path,
    uint vertex_count,
    uint32_t triangle_size,
    bool normal, bool tangent,
    uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool build_mesh,
    bool calculate_bound,
    uint64_t file_offset,
    bool copy_to_host,
    uint64_t extra_data_size) {
    _async_load(FileLoad{path, file_offset}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), build_mesh, calculate_bound, triangle_size, copy_to_host, extra_data_size);
}

void DeviceMesh::create_mesh(
    CommandList &cmdlist,
    uint vertex_count, bool normal, bool tangent, uint uv_count, uint triangle_count, vstd::vector<uint> &&submesh_triangle_offset) {
    LUISA_ASSERT(_render_mesh_data == nullptr && _gpu_load_frame == 0, "Device mesh already loaded.");
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    _render_mesh_data = sm.mesh_manager().load_mesh(
        sm.bindless_allocator(),
        cmdlist,
        sm.host_upload_buffer(),
        render_device.lc_device().create_buffer<uint>(get_mesh_size(vertex_count, normal, tangent, uv_count, triangle_count) / sizeof(uint)),
        {}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset),
        false);
}
void DeviceMesh::calculate_bounding_box() {
    LUISA_ASSERT(_render_mesh_data, "Mesh data not loaded.");
    LUISA_ASSERT(!_host_data.empty(), "Host data not loaded.");
    auto indices = (uint *)(_host_data.data() + _render_mesh_data->meta.tri_byte_offset);
    auto verts = (float3 *)_host_data.data();
    auto &bbox_request = _render_mesh_data->bbox_requests;
    if (!bbox_request) {
        bbox_request = new MeshManager::BBoxRequest();
    }
    // luisa::fiber::schedule
    auto calculate_bound = [&](luisa::span<std::atomic<float>> boundings) {
        luisa::fiber::parallel(
            (uint64_t)0,
            (uint64_t)(_render_mesh_data->triangle_size * 3),
            1024,
            [&](uint64_t begin, uint64_t end) {
                auto &submesh_offset = _render_mesh_data->submesh_offset;
                uint submesh_index = 1;
                if (!submesh_offset.empty()) {
                    submesh_index =
                        binary_search(
                            luisa::span{submesh_offset},
                            (uint)(begin / 3u) + 1,
                            [](auto a, auto b) {
                                if (a < b) return -1;
                                return a > b ? 1 : 0;
                            });
                }
                uint next_submesh =
                    submesh_index < submesh_offset.size() ?
                        submesh_offset[submesh_index] :
                        _render_mesh_data->triangle_size;
                next_submesh *= 3;
                float3 min_value(1e28f);
                float3 max_value(-1e28f);
                for (auto i = begin; i < end; ++i) {
                    if (i >= next_submesh) {
                        auto ptr = boundings.data() + (submesh_index - 1) * 6;
                        for (auto j = 0; j < 3; ++j) {
                            atomic_min(ptr[j], min_value[j]);
                            atomic_max(ptr[j + 3], max_value[j]);
                        }
                        submesh_index += 1;
                        next_submesh =
                            submesh_index < submesh_offset.size() ?
                                submesh_offset[submesh_index] :
                                _render_mesh_data->triangle_size;
                        next_submesh *= 3;
                        min_value = float3(1e28f);
                        max_value = float3(-1e28f);
                    }
                    auto &vert = verts[indices[i]];
                    min_value = min(min_value, vert);
                    max_value = max(max_value, vert);
                }
                auto ptr = boundings.data() + (submesh_index - 1) * 6;
                for (auto j = 0; j < 3; ++j) {
                    atomic_min(ptr[j], min_value[j]);
                    atomic_max(ptr[j + 3], max_value[j]);
                }
            });
    };
    if constexpr (sizeof(std::atomic<float>) == sizeof(float)) {
        bbox_request->bounding_box.clear();
        vstd::push_back_all(
            bbox_request->bounding_box,
            std::max<uint64_t>(1, _render_mesh_data->submesh_offset.size()),
            AABB{.packed_min = {1e28f, 1e28f, 1e28f},
                 .packed_max = {-1e28f, -1e28f, -1e28f}});
        auto float_size = bbox_request->bounding_box.size() * sizeof(AABB) / sizeof(float);
        calculate_bound(
            {// type force change, need launder here
             std::launder((std::atomic<float> *)bbox_request->bounding_box.data()),
             float_size});
    } else {
        luisa::vector<std::atomic<float>> atomic_data;
        bbox_request->bounding_box.clear();
        auto bounding_count = std::max<uint64_t>(1, _render_mesh_data->submesh_offset.size());
        bbox_request->bounding_box.resize(bounding_count);
        constexpr auto aabb_size = sizeof(AABB) / sizeof(float);
        auto float_size = bbox_request->bounding_box.size() * aabb_size;
        atomic_data.resize(float_size);
        for (auto i : vstd::range(bounding_count)) {
            atomic_data[i * aabb_size] = 1e28;
            atomic_data[i * aabb_size + 1] = 1e28;
            atomic_data[i * aabb_size + 2] = 1e28;
            atomic_data[i * aabb_size + 3] = -1e28;
            atomic_data[i * aabb_size + 4] = -1e28;
            atomic_data[i * aabb_size + 5] = -1e28;
        }
        calculate_bound(atomic_data);
        for (auto i : vstd::range(bounding_count)) {
            auto &ab = bbox_request->bounding_box[i];
            for (auto j : vstd::range(3)) {
                ab.packed_min[j] = atomic_data[i * aabb_size + j];
                ab.packed_max[j] = atomic_data[i * aabb_size + j + 3];
            }
        }
    }
    bbox_request->finished = true;
}
void DeviceMesh::set_bounding_box(luisa::span<AABB const> bounding_box) {
    LUISA_ASSERT(_render_mesh_data, "Mesh data not loaded.");
    LUISA_ASSERT(bounding_box.size() == std::max<uint64_t>(_render_mesh_data->submesh_offset.size(), 1), "Bounding box size mismatch with submesh size");
    auto &bbox_request = _render_mesh_data->bbox_requests;
    if (!bbox_request) {
        bbox_request = new MeshManager::BBoxRequest();
    }
    bbox_request->finished = true;
    bbox_request->mesh_data = _render_mesh_data;
    bbox_request->bounding_box.clear();
    vstd::push_back_all(bbox_request->bounding_box, bounding_box);
}
uint64_t DeviceMesh::get_mesh_size(uint32_t vertex_count, bool contained_normal, bool contained_tangent, uint32_t uv_count, uint32_t triangle_count) {
    uint64_t size = vertex_count * sizeof(float3);
    if (contained_normal) {
        size += vertex_count * sizeof(float3);
    }
    if (contained_tangent) {
        size += vertex_count * sizeof(float4);
    }
    size += uv_count * vertex_count * sizeof(float2);
    size += triangle_count * sizeof(Triangle);
    return size;
}
}// namespace rbc