#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_io/io_command_list.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc
{
DeviceMesh::DeviceMesh()
{
}
DeviceMesh::~DeviceMesh()
{
    if (_render_mesh_data)
    {
        auto inst = AssetsManager::instance();
        if (inst)
        {
            inst
                ->scene_mng()
                ->mesh_manager()
                .emplace_unload_mesh_cmd(_render_mesh_data);
        }
    }
}

template <typename LoadType>
void DeviceMesh::_async_load(
    LoadType&& load_type,
    uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint>&& submesh_triangle_offset,
    bool build_mesh, bool calculate_bound
)
{
    _contained_normal = normal;
    _contained_tangent = tangent;
    _uv_count = uv_count;
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]]
    {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push(
        [vertex_count, build_mesh, calculate_bound, normal, tangent, uv_count, this_shared = RC{ this }, submesh_triangle_offset = std::move(submesh_triangle_offset), load_type = std::move(load_type)](LoadTaskArgs const& args) mutable {
            auto ptr = static_cast<DeviceMesh*>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            vstd::optional<AccelOption> opt;
            if (build_mesh)
                opt.create(AccelOption{ .allow_compaction = false });
            /////////// Load as file
            if constexpr (std::is_same_v<LoadType, FileLoad>)
            {
                luisa::filesystem::path const& path = load_type.path;
                size_t file_offset = load_type.file_offset;
                auto file = args.io_cmdlist.retrieve(luisa::to_string(path));
                if (!file) [[unlikely]]
                {
                    LUISA_ERROR("DeviceMesh path {} not found.", luisa::to_string(path));
                }
                auto data_buffer = inst->lc_device().create_buffer<uint>((file.length() + sizeof(uint) - 1) / sizeof(uint));
                args.io_cmdlist << IOCommand(
                    file,
                    file_offset,
                    IOBufferSubView(data_buffer)
                );

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
                    submesh_triangle_offset,
                    calculate_bound
                );
            }
            /////////// Load as memory
            else if constexpr (std::is_same_v<LoadType, MemoryLoad>)
            {
                BinaryBlob const& data = load_type.blob;
                auto data_buffer = inst->lc_device().create_buffer<uint>((data.size() + sizeof(uint) - 1) / sizeof(uint));
                args.mem_io_cmdlist << IOCommand(
                    data.data(),
                    0,
                    IOBufferSubView(data_buffer)
                );
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
                    submesh_triangle_offset,
                    true
                );
            }
            /////////// Load as Buffer
            else if constexpr (std::is_same_v<LoadType, BufferLoad>)
            {
                Buffer<uint>& data_buffer = load_type.buffer;
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
                    submesh_triangle_offset
                );
            }
        }
    );
}

void DeviceMesh::async_load_from_buffer(
    Buffer<uint>&& data,
    uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint>&& submesh_triangle_offset,
    bool calculate_bound
)
{
    _async_load(BufferLoad{ std::move(data) }, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), true, calculate_bound);
}

void DeviceMesh::async_load_from_memory(
    BinaryBlob&& data,
    uint vertex_count, bool normal, bool tangent, uint uv_count, vstd::vector<uint>&& submesh_triangle_offset,
    bool build_mesh,
    bool calculate_bound
)
{
    _async_load(MemoryLoad{ std::move(data) }, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), build_mesh, calculate_bound);
}

void DeviceMesh::async_load_from_file(
    luisa::filesystem::path const& path,
    size_t file_offset,
    uint vertex_count, bool normal, bool tangent,
    uint uv_count, vstd::vector<uint>&& submesh_triangle_offset,
    bool build_mesh,
    bool calculate_bound
)
{
    _async_load(FileLoad{ path, file_offset }, vertex_count, normal, tangent, uv_count, std::move(submesh_triangle_offset), build_mesh, calculate_bound);
}
} // namespace rbc