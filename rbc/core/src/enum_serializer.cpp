#include <rbc_core/enum_serializer.h>
#include <luisa/core/logging.h>
namespace rbc {
EnumSerializer::EnumSerializer(std::initializer_list<char const *> names, std::initializer_list<uint32_t> values) {
    LUISA_DEBUG_ASSERT(names.size() == values.size());
    for (size_t i = 0; i < names.size(); ++i) {
        key_values.try_emplace(names.begin()[i], values.begin()[i]);
    }
}
EnumSerializer::~EnumSerializer() {}
luisa::optional<uint32_t> EnumSerializer::get_value(char const *name) {
    auto iter = key_values.find(luisa::string_view(name));
    if (iter != key_values.end())
        return iter->second;
    return {};
}
}// namespace rbc