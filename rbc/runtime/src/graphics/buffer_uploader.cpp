#include <rbc_graphics/buffer_uploader.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/logging.h>
#include <luisa/dsl/sugar.h>
#include <rbc_graphics/shader_manager.h>

namespace rbc {
BufferUploader::BufferUploader() {}
void BufferUploader::load_shader(luisa::fiber::counter &counter) {
    ShaderManager::instance()->async_load(counter, "texture_process/uploader_align2cp2.bin", _align2_copy);
    ShaderManager::instance()->async_load(counter, "texture_process/uploader_align4cp2.bin", _align4_copy);
    ShaderManager::instance()->async_load(counter, "texture_process/uploader_align8cp2.bin", _align8_copy);
    ShaderManager::instance()->async_load(counter, "texture_process/uploader_align16cp2.bin", _align16_copy);
}

void BufferUploader::commit_cmd(
    CmdKey const &key, CmdValue &value,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer) {
    auto ind_buffer = temp_buffer.allocate_upload_buffer<uint>(value.indices.size(), 16);
    memcpy(ind_buffer.mapped_ptr(), value.indices.data(), value.indices.size_bytes());
    auto alloc_data_buffer = [&]<typename T>() {
        auto data_buffer = temp_buffer.allocate_upload_buffer<T>((value.datas.size_bytes() + sizeof(T) - 1) / sizeof(T), 16);
        memcpy(data_buffer.mapped_ptr(), value.datas.data(), value.datas.size_bytes());
        return data_buffer;
    };
    switch (key.struct_align) {
        case 2: {
            cmdlist << (*_align2_copy)(
                           value.origin_buffer.as<uint16_t>(),
                           alloc_data_buffer.operator()<uint>().view.as<uint16_t>(),
                           ind_buffer.view)
                           .dispatch(value.indices.size(), key.struct_size / 2u);
        } break;
        case 4: {
            cmdlist << (*_align4_copy)(
                           value.origin_buffer.as<uint>(),
                           alloc_data_buffer.operator()<uint>().view,
                           ind_buffer.view)
                           .dispatch(value.indices.size(), key.struct_size / 4u);
        } break;
        case 8: {
            cmdlist << (*_align8_copy)(
                           value.origin_buffer.as<uint2>(),
                           alloc_data_buffer.operator()<uint2>().view,
                           ind_buffer.view)
                           .dispatch(value.indices.size(), key.struct_size / 8u);
        } break;
        case 16: {
            cmdlist << (*_align16_copy)(
                           value.origin_buffer.as<uint4>(),
                           alloc_data_buffer.operator()<uint4>().view,
                           ind_buffer.view)
                           .dispatch(value.indices.size(), key.struct_size / 16u);
        } break;
        default:
            LUISA_ERROR("Invalid alignment {}, require 2, 4, 8 or 16.", key.struct_align);
            break;
    }
}

bool BufferUploader::commit(
    CommandList &cmdlist,
    HostBufferManager &temp_buffer) {
    bool non_empty = false;
    for (auto &map : _copy_cmd) {
        for (auto &i : map.second) {
            non_empty = true;
            commit_cmd(i.first, i.second, cmdlist, temp_buffer);
        }
    }
    _copy_cmd.clear();
    return non_empty;
}

void BufferUploader::_swap_buffer(
    uint64 old_buffer_handle,
    BufferView<uint> buffer) {
    auto iter = _copy_cmd.find(old_buffer_handle);
    if (!iter) return;
    auto map = std::move(iter.value());
    for (auto &i : map) {
        i.second.origin_buffer = buffer;
    }
    _copy_cmd.remove(iter);
    _copy_cmd.emplace(buffer.handle(), std::move(map));
}

auto BufferUploader::_get_copy_cmd(
    BufferView<uint> origin_buffer,
    uint64 struct_size,
    uint64 struct_align) -> CmdValue & {
    auto &map = _copy_cmd.emplace(origin_buffer.handle()).value();
    CmdKey key{
        struct_size,
        struct_align};
    auto iter = map.try_emplace(
        key,
        vstd::lazy_eval([&]() {
            return CmdValue{
                origin_buffer};
        }));
    return iter.first->second;
}

void *BufferUploader::emplace_copy_cmd(
    BufferView<uint> origin_buffer,
    uint64 struct_size,
    uint64 struct_align,
    uint64 offset,
    uint64 size) {
    auto &map = _copy_cmd.emplace(origin_buffer.handle()).value();
    CmdKey key{
        struct_size,
        struct_align};
    auto iter = map.try_emplace(
        key,
        vstd::lazy_eval([&]() {
            return CmdValue{
                origin_buffer};
        }));
    auto &v = iter.first->second;
    for (auto i : vstd::range(offset, offset + size)) {
        v.indices.emplace_back(i);
    }
    auto start_idx = v.datas.size();
    v.datas.push_back_uninitialized(size * struct_size);
    return v.datas.data() + start_idx;
}

void BufferUploader::emplace_copy_cmd(
    BufferView<uint> origin_buffer,
    uint64 struct_size,
    uint64 struct_align,
    uint64 offset,
    uint64 size,
    void const *data) {
    auto host_ptr = emplace_copy_cmd(origin_buffer, struct_size, struct_align, offset, size);
    memcpy(host_ptr, data, size * struct_size);
}

BufferUploader::~BufferUploader() {
}
}// namespace rbc