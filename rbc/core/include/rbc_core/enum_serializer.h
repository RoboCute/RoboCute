#pragma once
#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/stl/optional.h>
#include <initializer_list>
#include <rbc_config.h>
namespace rbc {
struct RBC_CORE_API EnumSerializer {
    luisa::unordered_map<luisa::string, uint32_t> key_values;
    EnumSerializer(std::initializer_list<char const *> names, std::initializer_list<uint32_t> values);
    ~EnumSerializer();
    luisa::optional<uint32_t> get_value(char const *name);
};
}// namespace rbc