#include <rbc_world/callback_serializer.h>
#include <luisa/vstl/common.h>
#include <rbc_core/shared_atomic_mutex.h>
#include <shared_mutex>

namespace rbc::world {

namespace {
rbc::shared_atomic_mutex _ser_callback_mtx;
vstd::HashMap<luisa::string, luisa::move_only_function<void(void *)>> _ser_callback;
}// namespace

RBC_RUNTIME_API void regist_callback(
    luisa::string_view name,
    luisa::move_only_function<void(void *)> &&callback) {
    std::lock_guard lck{_ser_callback_mtx};
    _ser_callback.emplace(name, std::move(callback));
}

RBC_RUNTIME_API void unregist_callback(luisa::string_view name) {
    std::lock_guard lck{_ser_callback_mtx};
    _ser_callback.remove(name);
}

[[nodiscard]] RBC_RUNTIME_API luisa::move_only_function<void(void *)> *get_callback(
    luisa::string_view name) {
    std::shared_lock lck{_ser_callback_mtx};
    auto iter = _ser_callback.find(name);
    if (!iter) return nullptr;
    return &iter.value();
}

}// namespace rbc::world
