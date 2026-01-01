#pragma once
#include <luisa/core/stl/unordered_map.h>
#include <luisa/runtime/buffer.h>

namespace rbc::ps {

struct BufferProxy {
    template<typename T>
    using BufferView = luisa::compute::BufferView<T>;
    using Type = luisa::compute::Type;
    BufferView<uint> uint_view;// offset_bytes, size_bytes, handle
    Type const *p_type;        // p_type->alignment to get actual stride
};

struct BufferInstance {
    template<typename T>
    using Buffer = luisa::compute::Buffer<T>;
    using uint = luisa::uint;
    using Type = luisa::compute::Type;
    Buffer<uint> buffer;
    Type const *p_type;
};

}// namespace rbc::ps