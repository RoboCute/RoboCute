#include <rbc_core/enum_serializer.h>
#include <luisa/core/logging.h>
#include <luisa/vstl/common.h>
namespace rbc {
static EnumSerializer &enum_ser_singleton() {
    static EnumSerializer _enum_serializer;
    return _enum_serializer;
}
EnumSerIniter::EnumSerIniter(
    luisa::string_view enum_name,
    std::initializer_list<char const *> names,
    std::initializer_list<uint64_t> numbers) {
    LUISA_DEBUG_ASSERT(names.size() == numbers.size());
    auto &r = enum_ser_singleton();
    luisa::string enum_str{enum_name};
    for (size_t i = 0; i < names.size(); ++i) {
        r.enum_value_to_name.try_emplace(
            EnumReflectionType{
                enum_str,
                numbers.begin()[i]},
            names.begin()[i]);
        luisa::string combined_name = enum_str + "##" + names.begin()[i];
        r.enum_name_to_value.try_emplace(
            std::move(combined_name),
            numbers.begin()[i]);
    }
}

luisa::string_view EnumSerializer::_get_enum_value_name(
    luisa::string_view enum_name,
    uint64_t value) {
    auto &r = enum_ser_singleton();
    auto iter = r.enum_value_to_name.find(EnumReflectionType{
        luisa::string{enum_name},
        value});
    if (iter == r.enum_value_to_name.end()) [[unlikely]] {
        LUISA_ERROR("Enum {} with value {} not serialized properly.", enum_name, value);
    }
    return iter->second;
}

vstd::optional<uint64_t> EnumSerializer::_get_enum_value(
    luisa::string_view enum_type,
    luisa::string_view enum_value) {
    luisa::fixed_vector<char, 64> combined_name;
    combined_name.reserve(enum_type.size() + 2 + enum_value.size());
    vstd::push_back_all(
        combined_name,
        enum_type.data(),
        enum_type.size());
    vstd::push_back_all(
        combined_name,
        "##",
        2);
    vstd::push_back_all(
        combined_name,
        enum_value.data(),
        enum_value.size());
    luisa::string_view combined_name_view{combined_name.data(), combined_name.size()};
    auto &r = enum_ser_singleton();
    auto iter = r.enum_name_to_value.find(combined_name_view);
    if (iter == r.enum_name_to_value.end()) return {};
    return iter->second;
}
}// namespace rbc