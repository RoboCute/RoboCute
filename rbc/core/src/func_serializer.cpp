#include <rbc_core/func_serializer.h>
namespace rbc {
namespace func_ser_detail {
static vstd::HashMap<vstd::Guid::GuidData, FuncSerializer::FuncCall> func_maps;
}// namespace func_ser_detail

FuncSerializer::FuncSerializer(
    std::initializer_list<char const *> names,
    std::initializer_list<AnyFuncPtr> funcs,
    std::initializer_list<HeapObjectMeta> args_meta,
    std::initializer_list<HeapObjectMeta> ret_value_meta,
    std::initializer_list<bool> is_static) {
    LUISA_DEBUG_ASSERT(names.size() == funcs.size() && names.size() == args_meta.size() && names.size() == ret_value_meta.size() && names.size() == is_static.size());
    func_ser_detail::func_maps.reserve(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        auto guid = vstd::Guid::TryParseGuid(names.begin()[i]);
        LUISA_DEBUG_ASSERT(guid);
        func_ser_detail::func_maps.emplace(
            guid->to_binary(),
            funcs.begin()[i],
            args_meta.begin()[i],
            ret_value_meta.begin()[i],
            is_static.begin()[i]);
    }
}
FuncSerializer::~FuncSerializer() = default;
auto FuncSerializer::get_call_meta(vstd::Guid const &name) -> FuncCall const * {
    auto const iter = func_ser_detail::func_maps.find(name.to_binary());
    if (!iter) [[unlikely]]
        return nullptr;
    return &iter.value();
}
}// namespace rbc