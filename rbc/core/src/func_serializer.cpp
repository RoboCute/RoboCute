#include <rbc_core/func_serializer.h>
namespace rbc {
namespace func_ser_detail {
static luisa::unordered_map<luisa::string, luisa::unique_ptr<FuncSerializer::FuncCall>> func_maps;
}// namespace func_ser_detail

FuncSerializer::FuncSerializer(
    std::initializer_list<const char *> names,
    std::initializer_list<ClousureType> funcs,
    std::initializer_list<HeapObjectMeta> args_meta,
    std::initializer_list<HeapObjectMeta> ret_value_meta) {
    LUISA_DEBUG_ASSERT(names.size() == funcs.size() && names.size() == args_meta.size() && names.size() == ret_value_meta.size());
    func_ser_detail::func_maps.reserve(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        func_ser_detail::func_maps.try_emplace(
            luisa::string{
                names.begin()[i]},
            luisa::make_unique<FuncCall>(
                funcs.begin()[i],
                args_meta.begin()[i],
                ret_value_meta.begin()[i]));
    }
}
FuncSerializer::~FuncSerializer() {}
bool FuncSerializer::try_call(
    luisa::string_view name,
    void *self,
    void *args,
    void *ret_value) {
    auto iter = func_ser_detail::func_maps.find(name);
    if (iter == func_ser_detail::func_maps.end()) [[unlikely]]
        return false;
    iter->second->func(self, args, ret_value);
    return true;
}
auto FuncSerializer::get_call_meta(luisa::string_view name) -> FuncCall const * {
    auto iter = func_ser_detail::func_maps.find(name);
    if (iter == func_ser_detail::func_maps.end()) [[unlikely]]
        return nullptr;
    return iter->second.get();
}
}// namespace rbc