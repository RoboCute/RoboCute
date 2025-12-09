#include <rbc_world_v2/type_register.h>
#include <rbc_core/serde.h>
namespace rbc::world {
BaseObject::BaseObject() {
    _guid.reset();
}
void BaseObject::rbc_objser(rbc::JsonSerializer &obj) const {
    LUISA_DEBUG_ASSERT(_guid.to_binary().data0 != 0 && _guid.to_binary().data1 != 0);
    auto tid = type_id();
    obj._store(reinterpret_cast<vstd::Guid &>(tid), "__type_id__");
    obj._store(_guid, "_guid");
    serialize(obj);
}
void BaseObject::rbc_objdeser(rbc::JsonDeSerializer &obj) {
    obj._load(_guid, "_guid");
    deserialize(obj);
}
namespace detail {
struct GuidHashEqual {
    using T = std::array<uint64_t, 2>;
    size_t operator()(T const &a, size_t seed = luisa::hash64_default_seed) const {
        return luisa::hash64(a.data(), a.size() * sizeof(uint64_t), seed);
    }
    bool operator()(T const &a, T const &b) const {
        return a[0] == b[0] && a[1] == b[1];
    }
};
}// namespace detail
struct TypeRegisterSingleton : vstd::IOperatorNewBase {
    luisa::unordered_map<std::array<uint64_t, 2>, TypeRegister::CreateFunc, detail::GuidHashEqual> _create_funcs;
};
static TypeRegisterSingleton *_type_reg_singleton{};
void TypeRegister::init_instance() {
    LUISA_DEBUG_ASSERT(!_type_reg_singleton);
    _type_reg_singleton = new TypeRegisterSingleton{};
}
void TypeRegister::destroy_instance() {
    delete _type_reg_singleton;
    _type_reg_singleton = nullptr;
}
TypeRegister::TypeRegister(
    std::array<uint64_t, 2> guid,
    CreateFunc create_obj) {
    _type_reg_singleton->_create_funcs.try_emplace(guid, create_obj);
}
BaseObject *BaseObject::create_object(vstd::Guid const &guid) {
    auto iter = _type_reg_singleton->_create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(guid));
    if (iter == _type_reg_singleton->_create_funcs.end()) {
        return nullptr;
    }
    return iter->second();
}
BaseObject *BaseObject::deserialize_object(rbc::JsonDeSerializer &obj) {
    vstd::Guid type_id;
    if (!obj._load(type_id, "__type_id__")) [[unlikely]]
        return nullptr;
    auto ptr = create_object(type_id);
    if (!ptr) [[unlikely]]
        return nullptr;
    ptr->rbc_objdeser(obj);
}
}// namespace rbc::world