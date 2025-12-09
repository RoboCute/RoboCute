#include <rbc_world_v2/world_plugin.h>
#include "type_register.h"
#include <rbc_world_v2/transform.h>
namespace rbc::world {
struct Transform;
static TypeRegisterBase *_type_register_header{};
void type_regist_init_mark(TypeRegisterBase *type_register) {
    type_register->p_next = _type_register_header;
    _type_register_header = type_register;
}
luisa::vector<InstanceID> &dirty_transforms();
struct WorldPluginImpl : WorldPlugin {
    luisa::unordered_map<std::array<uint64_t, 2>, TypeRegisterBase *> _create_funcs;
    WorldPluginImpl() {
        for (auto p = _type_register_header; p; p = p->p_next) {
            _create_funcs.try_emplace(p->type_id(), p);
        }
    }
    ~WorldPluginImpl() {
    }
    BaseObject *create_object(vstd::Guid const &guid) {
        auto iter = _create_funcs.find(
            reinterpret_cast<std::array<uint64_t, 2> const &>(guid));
        if (iter == _create_funcs.end()) {
            return nullptr;
        }
        return iter->second->create();
    }
    BaseObject *create_object(rbc::TypeInfo const &type_info) override {
        auto md5 = type_info.md5();
        return create_object(reinterpret_cast<vstd::Guid const &>(md5));
    }
    BaseObject *deserialize_object(rbc::JsonDeSerializer &obj) override {
        vstd::Guid type_id;
        if (!obj._load(type_id, "__type_id__")) [[unlikely]]
            return nullptr;
        auto ptr = create_object(type_id);
        if (!ptr) [[unlikely]]
            return nullptr;
        ptr->rbc_objdeser(obj);
        return ptr;
    }
    BaseObject *get_object(InstanceID instance_id) const override {
        return BaseObject::get_object(instance_id);
    }
    [[nodiscard]] luisa::span<InstanceID const> get_dirty_transforms() const override {
        return dirty_transforms();
    }
    void clear_dirty_transform() const override {
        auto &v = dirty_transforms();
        for (auto &i : v) {
            auto obj = get_object(i);
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<Transform>()));
            static_cast<Transform *>(obj)->_dirty = false;
        }
        v.clear();
    }
};
static WorldPluginImpl world_plugin_singleton{};
LUISA_EXPORT_API WorldPlugin *get_world_plugin() {
    return &world_plugin_singleton;
}
}// namespace rbc::world