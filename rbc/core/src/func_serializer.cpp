#include <rbc_core/func_serializer.h>
namespace rbc {
namespace func_ser_detail {
struct GuidHash {
    size_t operator()(vstd::Guid::GuidData const &v) const {
        return luisa::hash64(&v, sizeof(vstd::Guid::GuidData), luisa::hash64_default_seed);
    }
    bool operator()(vstd::Guid::GuidData const a, vstd::Guid::GuidData const b) const {
        return a.data0 == b.data0 && a.data1 == b.data1;
    }
};
static luisa::unordered_map<vstd::Guid::GuidData, luisa::unique_ptr<FuncSerializer::FuncCall>, GuidHash, GuidHash> func_maps;
}// namespace func_ser_detail

FuncSerializer::FuncSerializer(
    std::initializer_list<const char *> names,
    std::initializer_list<AnyFuncPtr> funcs,
    std::initializer_list<HeapObjectMeta> args_meta,
    std::initializer_list<HeapObjectMeta> ret_value_meta,
    std::initializer_list<bool> is_static) {
    LUISA_DEBUG_ASSERT(names.size() == funcs.size() && names.size() == args_meta.size() && names.size() == ret_value_meta.size() && names.size() == is_static.size());
    func_ser_detail::func_maps.reserve(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        auto guid = vstd::Guid::TryParseGuid(names.begin()[i]);
        LUISA_DEBUG_ASSERT(guid);
        func_ser_detail::func_maps.try_emplace(
            guid->to_binary(),
            luisa::make_unique<FuncCall>(
                funcs.begin()[i],
                args_meta.begin()[i],
                ret_value_meta.begin()[i],
                is_static.begin()[i]));
    }
}
FuncSerializer::~FuncSerializer() {}
auto FuncSerializer::get_call_meta(vstd::Guid const &name) -> FuncCall const * {
    auto iter = func_ser_detail::func_maps.find(name.to_binary());
    if (iter == func_ser_detail::func_maps.end()) [[unlikely]]
        return nullptr;
    return iter->second.get();
}
}// namespace rbc