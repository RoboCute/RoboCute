#pragma once
#include <rbc_config.h>
#include <luisa/vstl/functional.h>
namespace rbc::world {
RBC_RUNTIME_API void regist_callback(
    luisa::string_view name,
    luisa::move_only_function<void(void *)> &&callback);
RBC_RUNTIME_API void unregist_callback(
    luisa::string_view name);
RBC_RUNTIME_API luisa::move_only_function<void(void *)> *get_callback(
    luisa::string_view name);
}// namespace rbc::world