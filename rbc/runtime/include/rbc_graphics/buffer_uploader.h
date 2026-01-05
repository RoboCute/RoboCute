#pragma once
#include <rbc_config.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/core/fiber.h>
#include "host_buffer_manager.h"
namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct RBC_RUNTIME_API BufferUploader
{
public:
    struct CmdValue {
        BufferView<uint> origin_buffer;
        vector<uint> indices;
        vector<std::byte> datas;
    };

private:
    Shader2D<Buffer<uint16_t>, Buffer<uint16_t>, Buffer<uint>> const* _align2_copy{ nullptr };
    Shader2D<Buffer<uint>, Buffer<uint>, Buffer<uint>> const* _align4_copy{ nullptr };
    Shader2D<Buffer<uint2>, Buffer<uint2>, Buffer<uint>> const* _align8_copy{ nullptr };
    Shader2D<Buffer<uint4>, Buffer<uint4>, Buffer<uint>> const* _align16_copy{ nullptr };
    struct CmdKey {
        uint64 struct_size;
        uint64 struct_align;
        bool operator==(CmdKey const& key) const
        {
            return struct_size == key.struct_size && struct_align == key.struct_align;
        }
    };
    struct CmdKeyHash {
        size_t operator()(CmdKey const& key) const
        {
            return luisa::hash64(&key, sizeof(CmdKey), luisa::hash64_default_seed);
        }
    };

    vstd::HashMap<uint64, vstd::unordered_map<CmdKey, CmdValue, CmdKeyHash>> _copy_cmd;
    void commit_cmd(
        CmdKey const& key, CmdValue& value,
        CommandList& cmdlist,
        HostBufferManager& temp_buffer
    );
    void _swap_buffer(
        uint64 old_buffer_handle,
        BufferView<uint> buffer
    );

public:
    BufferUploader();
    void load_shader(luisa::fiber::counter& counter);
    void* emplace_copy_cmd(
        BufferView<uint> origin_buffer,
        uint64 struct_size,
        uint64 struct_align,
        uint64 offset,
        uint64 size
    );
    CmdValue& _get_copy_cmd(
        BufferView<uint> origin_buffer,
        uint64 struct_size,
        uint64 struct_align
    );
    template <typename T>
    CmdValue& _get_copy_cmd(BufferView<T> dst_buffer)
    {
        return _get_copy_cmd(
            dst_buffer.original().template as<uint>(), sizeof(T), alignof(T)
        );
    }
    void emplace_copy_cmd(
        BufferView<uint> origin_buffer,
        uint64 struct_size,
        uint64 struct_align,
        uint64 offset,
        uint64 size,
        void const* data
    );
    template <typename T>
    T* emplace_copy_cmd(BufferView<T> dst_buffer)
    {
        return reinterpret_cast<T*>(
            emplace_copy_cmd(dst_buffer.original().template as<uint>(), sizeof(T), alignof(T), dst_buffer.offset(), dst_buffer.size())
        );
    }
    template <typename T>
    void emplace_copy_cmd(
        BufferView<T> dst_buffer,
        T const* data
    )
    {
        emplace_copy_cmd(dst_buffer.original().template as<uint>(), sizeof(T), alignof(T), dst_buffer.offset(), dst_buffer.size(), data);
    }
    bool commit(
        CommandList& cmdlist,
        HostBufferManager& temp_buffer
    );
    template <typename A, typename B>
    void swap_buffer(
        Buffer<A> const& old_buffer,
        Buffer<B> const& new_buffer
    )
    {
        _swap_buffer(old_buffer.handle(), new_buffer.view().template as<uint>());
    }
    ~BufferUploader();
};
} // namespace rbc