#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_io/io_command_list.h>
#include <rbc_graphics/scene_manager.h>
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
    uint64_t file_size,
    bool copy_to_host) {
    _contained_normal = normal;
    _contained_tangent = tangent;
    _uv_count = uv_count;
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push(
        [vertex_count, build_mesh, calculate_bound, copy_to_host, file_size, normal, tangent, uv_count, this_shared = RC{this}, submesh_triangle_offset = std::move(submesh_triangle_offset), load_type = std::move(load_type)](LoadTaskArgs const &args) mutable {
            auto ptr = static_cast<DeviceMesh *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            vstd::optional<AccelOption> opt;
            if (build_mesh)
                opt.create(AccelOption{.allow_compaction = false});
            /////////// Load as file
            if constexpr (std::is_same_v<LoadType, FileLoad>) {
                luisa::filesystem::path const &path = load_type.path;
                size_t file_offset = load_type.file_offset;
                auto file = args.io_cmdlist.retrieve(luisa::to_string(path));
                if (!file) [[unlikely]] {
                    LUISA_ERROR("DeviceMesh path {} not found.", luisa::to_string(path));
                }
                uint64_t size = (file.length() - file_offset) + sizeof(uint) - 1;
                size = std::min(size, file_size);
                auto data_buffer = inst->lc_device().create_buffer<uint>(size / sizeof(uint));
                if (copy_to_host) {
                    *args.require_disk_io_sync = true;
                    auto &host_data = this_shared->_host_data;
                    host_data.clear();
                    host_data.push_back_uninitialized(data_buffer.size_bytes());
                    args.io_cmdlist << IOCommand(
                        file,
                        file_offset,
                        luisa::span{host_data});
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
                auto data_buffer = inst->lc_device().create_buffer<uint>((data.size() + sizeof(uint) - 1) / sizeof(uint));
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
    uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool calculate_bound) {
    _async_load(BufferLoad{std::move(data)}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), true, calculate_bound);
}

void DeviceMesh::async_load_from_memory(
    BinaryBlob &&data,
    uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool build_mesh,
    bool calculate_bound,
    bool copy_to_host) {
    if (copy_to_host) {
        _host_data.clear();
        _host_data.push_back_uninitialized(data.size());
        std::memcpy(_host_data.data(), data.data(), data.size());
        data = BinaryBlob{
            _host_data.data(),
            _host_data.size(),
            {}};
    }
    _async_load(MemoryLoad{std::move(data)}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), build_mesh, calculate_bound, ~0ull);
}

void DeviceMesh::async_load_from_file(
    luisa::filesystem::path const &path,
    uint vertex_count, bool normal, bool tangent,
    uint uv_count, vstd::vector<uint> &&submesh_triangle_offset,
    bool build_mesh,
    bool calculate_bound,
    uint64_t file_offset,
    uint64_t file_max_size,
    bool copy_to_host) {
    _async_load(FileLoad{path, file_offset}, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), build_mesh, calculate_bound, file_max_size, copy_to_host);
}

void DeviceMesh::create_mesh(
    CommandList &cmdlist,
    uint vertex_count, bool normal, bool tangent, uint uv_count, uint triangle_count, vstd::vector<uint> &&submesh_triangle_offset) {
    LUISA_ASSERT(_render_mesh_data == nullptr && _gpu_load_frame == 0, "Device mesh already loaded.");
    _contained_normal = normal;
    _contained_tangent = tangent;
    _uv_count = uv_count;
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
void DeviceMesh::set_bounding_box(luisa::span<AABB const> bounding_box) {
    LUISA_ASSERT(_render_mesh_data, "Mesh data not loaded.");
    LUISA_ASSERT(bounding_box.size() == std::max<size_t>(_render_mesh_data->submesh_offset.size(), 1), "Bounding box size mismatch with submesh size");
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
    size += uv_count * sizeof(float2);
    size += triangle_count * sizeof(Triangle);
    return size;
}
}// namespace rbc