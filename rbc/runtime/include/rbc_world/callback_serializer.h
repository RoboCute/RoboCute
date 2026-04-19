#pragma once
#include <rbc_config.h>
#include <luisa/vstl/functional.h>

namespace rbc::world {

/// Register a named callback in the global callback registry.
/// If a callback with the same name already exists, it will be replaced.
RBC_RUNTIME_API void regist_callback(
    luisa::string_view name,
    luisa::move_only_function<void(void *)> &&callback);

/// Unregister a named callback from the global callback registry.
/// If no callback with the given name exists, this is a no-op.
RBC_RUNTIME_API void unregist_callback(luisa::string_view name);

/// Look up a registered callback by name.
/// @return Pointer to the callback if found; nullptr otherwise.
/// @note The returned pointer is only valid while the registry is not modified.
[[nodiscard]] RBC_RUNTIME_API luisa::move_only_function<void(void *)> *get_callback(
    luisa::string_view name);

}// namespace rbc::world
