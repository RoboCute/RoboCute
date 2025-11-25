#pragma once
#include <rbc_config.h>
#include <rbc_graphics/buffer_uploader.h>
#include <luisa/runtime/device.h>
#include <luisa/core/logging.h>
#include <luisa/vstl/spin_mutex.h>
#include <typeindex>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/mat_code.h>
#include <rbc_graphics/materials.h>
#include <luisa/vstl/v_guid.h>
namespace rbc
{
struct BindlessAllocator;
namespace matmng_detail
{
#include "member_reflect.inl.h"
template <typename T>
struct is_std_array {
    static constexpr bool value = false;
};
template <typename T, size_t i>
struct is_std_array<std::array<T, i>> {
    static constexpr bool value = true;
};
template <typename T>
static constexpr bool is_std_array_v = is_std_array<T>::value;
} // namespace matmng_detail
struct RBC_RUNTIME_API MatManager {
public:
    struct MatType {
        Buffer<uint> data_buffer;
        uint size;
        uint align;
        uint instance_size;
        vstd::spin_mutex mtx;
        vector<uint> alloc_pool;
        vector<std::byte> host_data;
        MatType() = default;
        ~MatType() = default;
        MatType(MatType const&) = delete;
        MatType(MatType&&) = delete;
    };

private:
    Device& _device;
    vstd::HashMap<uint, MatType> _mats;
    bool _record_host_data;
    [[nodiscard]] uint _emplace_mat_type(
        BindlessAllocator& alloc,
        uint init_capacity,
        size_t size,
        size_t align,
        uint index
    );
    ///////// Thread safe
    ///////// Main thread only
    [[nodiscard]] MatCode _emplace_mat_instance(
        CommandList& cmdlist,
        uint mat_type,
        BindlessAllocator& alloc,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        span<const std::byte> mat_data
    );

public:
    MatManager(Device& device, bool record_host_data);
    MatManager(MatManager const&) = delete;
    using ImagePtr = vstd::variant<Image<float> const*, SparseImage<float> const*>;
    using VolumePtr = vstd::variant<Volume<float> const*, SparseVolume<float> const*>;
    ///////// Thread safe
    [[nodiscard]] auto instance_count(uint mat_type) const
    {
        auto iter = _mats.find(mat_type);
        LUISA_DEBUG_ASSERT(iter);
        return iter.value().instance_size;
    }
    [[nodiscard]] size_t get_mat_type_size(uint mat_type);
    void discard_mat_instance(
        MatCode mat_code
    );
    // must be host aware
    bool get_mat_instance(
        MatCode mat_code,
        luisa::span<std::byte> result
    );

    void set_mat_instance(
        MatCode mat_code,
        BufferUploader& uploader,
        span<const std::byte> mat_data
    );

    template <class U>
        requires (std::is_trivially_copyable_v<U> && std::is_trivially_destructible_v<U>)
    [[nodiscard]] uint emplace_mat_type(
        BindlessAllocator& alloc,
        uint init_capacity,
        uint mat_index
    )
    {
        return _emplace_mat_type(
            alloc,
            init_capacity,
            sizeof(U),
            alignof(U),
            mat_index
        );
    }

    template <class U>
        requires (std::is_trivially_copyable_v<U> && std::is_trivially_destructible_v<U>)
    [[nodiscard]] MatCode emplace_mat_instance(
        U& mat,
        CommandList& cmdlist,
        BindlessAllocator& alloc,
        BufferUploader& uploader,
        DisposeQueue& disp_queue,
        uint mat_index
    )
    {
        return _emplace_mat_instance(
            cmdlist,
            mat_index,
            alloc,
            uploader,
            disp_queue,
            span<const std::byte>(reinterpret_cast<const std::byte*>(&mat), sizeof(U))
        );
    }

    ///////// Main thread only

    ~MatManager();
};
} // namespace rbc