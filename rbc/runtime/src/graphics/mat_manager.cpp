#include <rbc_graphics/bindless_allocator.h>
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/logging.h>
namespace rbc
{
MatManager::MatManager(Device& device, bool record_host_data)
    : _device(device)
    , _record_host_data(record_host_data)
{
    _mats.reserve(256);
}
uint MatManager::_emplace_mat_type(
    BindlessAllocator& alloc,
    uint init_capacity,
    size_t size,
    size_t align,
    uint index
)
{
    LUISA_DEBUG_ASSERT(_mats.size() < 256, "Material type out of designed-range.");
    auto iter = _mats.try_emplace(index);
    if (!iter.second) return index;
    auto& v = iter.first.value();
    v.size = uint(size);
    v.align = uint(align);
    size_t bsize = size_t(init_capacity) * size;
    bsize = (bsize + 65535u) & (~65535u);
    v.data_buffer = _device.create_buffer<uint>(bsize / sizeof(uint));
    alloc.set_reserved_buffer(index, v.data_buffer);
    return index;
}
size_t MatManager::get_mat_type_size(uint mat_type_idx)
{
    auto mat_type_iter = _mats.find(mat_type_idx);
    LUISA_DEBUG_ASSERT(mat_type_iter);
    return mat_type_iter.value().size;
}
bool MatManager::get_mat_instance(
    MatCode mat_code,
    luisa::span<std::byte> result
)
{
    uint mat_type_idx = mat_code.get_type();
    auto mat_type_iter = _mats.find(mat_type_idx);
    LUISA_DEBUG_ASSERT(mat_type_iter);
    auto& mat_type = mat_type_iter.value();
    const auto struct_stride = mat_type.size;
    LUISA_DEBUG_ASSERT(result.size_bytes() == struct_stride);
    auto offset = static_cast<size_t>(mat_code.get_inst_id()) * struct_stride;
    std::lock_guard lck{ mat_type.mtx };
    if (mat_type.host_data.size() < offset + result.size_bytes())
    {
        return false;
    }
    std::memcpy(result.data(), mat_type.host_data.data() + offset, result.size_bytes());
    return true;
}
void MatManager::set_mat_instance(
    MatCode mat_code,
    BufferUploader& uploader,
    span<const std::byte> mat_data
)
{
    uint mat_type_idx = mat_code.get_type();
    auto id = mat_code.get_inst_id();
    auto mat_type_iter = _mats.find(mat_type_idx);
    LUISA_DEBUG_ASSERT(mat_type_iter);
    auto& mat_type = mat_type_iter.value();
    const auto struct_stride = mat_type.size;
    LUISA_DEBUG_ASSERT(mat_data.size() == struct_stride, "Material data size mismatch., data size: {}, type size: {}", mat_data.size(), struct_stride);

    auto ptr = reinterpret_cast<std::byte*>(uploader.emplace_copy_cmd(
        mat_type.data_buffer,
        struct_stride,
        mat_type.align,
        id,
        1
    ));
    std::memcpy(ptr, mat_data.data(), struct_stride);
    auto offset = static_cast<size_t>(mat_code.get_inst_id()) * struct_stride;
    std::lock_guard lck{ mat_type.mtx };
    if (mat_type.host_data.size() >= offset + struct_stride)
        std::memcpy(mat_type.host_data.data() + offset, mat_data.data(), struct_stride);
}

auto MatManager::_emplace_mat_instance(
    CommandList& cmdlist,
    uint mat_type_idx,
    BindlessAllocator& alloc,
    BufferUploader& uploader,
    DisposeQueue& disp_queue,
    span<const std::byte> mat_data
) -> MatCode
{
    auto mat_type_iter = _mats.find(mat_type_idx);
    auto& mat_type = mat_type_iter.value();
    const auto struct_stride = mat_type.size;
    uint id = ~0u;
    {
        std::lock_guard lck{ mat_type.mtx };
        if (!mat_type.alloc_pool.empty())
        {
            id = mat_type.alloc_pool.back();
            mat_type.alloc_pool.pop_back();
        }
        else if (_record_host_data)
        {
            mat_type.host_data.push_back_uninitialized(struct_stride);
        }
    }
    if (id == ~0u)
    {
        id = mat_type.instance_size++;
        // resize
        if (mat_type.instance_size * struct_stride > mat_type.data_buffer.size_bytes()) [[unlikely]]
        {
            RenderDevice::instance().lc_main_stream().synchronize();
            auto new_buffer = _device.create_buffer<uint>(mat_type.data_buffer.size() * 2);
            uploader.swap_buffer(mat_type.data_buffer, new_buffer);
            cmdlist << new_buffer.view(0, mat_type.data_buffer.size()).copy_from(mat_type.data_buffer);
            disp_queue.dispose_after_queue(std::move(mat_type.data_buffer));
            alloc.set_reserved_buffer(mat_type_idx, new_buffer);
            mat_type.data_buffer = std::move(new_buffer);
        }
    }
    MatCode result(mat_type_idx, id);
    set_mat_instance(result, uploader, mat_data);
    return result;
}
void MatManager::discard_mat_instance(
    MatCode mat_code
)
{
    uint mat_type_idx = mat_code.get_type();
    auto mat_type_iter = _mats.find(mat_type_idx);
    auto& mat_type = mat_type_iter.value();
    std::lock_guard lck{ mat_type.mtx };
    mat_type.alloc_pool.emplace_back(mat_code.get_inst_id());
}
MatManager::~MatManager()
{
}
} // namespace rbc