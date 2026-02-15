#pragma once

#include <luisa/vstl/common.h>
#include <luisa/vstl/spin_mutex.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/runtime/rtx/mesh.h>

namespace luisa::compute {

class DeviceInterface;

}// namespace luisa::compute

namespace rbc::lcapi_c {

class RefCounter : public vstd::IOperatorNewBase {
public:
    using Handle = uint64;
    using Disposer = std::pair<
        vstd::func_ptr_t<void(luisa::compute::DeviceInterface *, Handle)>,
        luisa::shared_ptr<luisa::compute::DeviceInterface>>;
    vstd::spin_mutex mtx;

private:
    vstd::unordered_map<Handle, std::pair<int64, Disposer>> refCounts;

public:
    RefCounter() noexcept;
    static RefCounter *current;
    ~RefCounter() noexcept;
    void AddObject(Handle handle, Disposer disposer) noexcept;
    void InRef(Handle handle) noexcept;
    void DeRef(Handle handle) noexcept;
};

}// namespace rbc::lcapi_c
