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
luisa::spin_mutex &dirty_trans_mtx();
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
    BaseObject *create_object(rbc::TypeInfo const &type_info) override {
        auto iter = _create_funcs.find(type_info.md5());
        if (iter == _create_funcs.end()) {
            return nullptr;
        }
        auto ptr = iter->second->create();
        ptr->init();
        return ptr;
    }
    BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid) override {
        auto iter = _create_funcs.find(type_info.md5());
        if (iter == _create_funcs.end()) {
            return nullptr;
        }
        auto ptr = iter->second->create();
        ptr->init_with_guid(guid);
        return ptr;
    }
    BaseObject *create_object(vstd::Guid const &type_info) override {
        auto iter = _create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
        if (iter == _create_funcs.end()) {
            return nullptr;
        }
        auto ptr = iter->second->create();
        ptr->init();
        return ptr;
    }
    BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid) override {
        auto iter = _create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
        if (iter == _create_funcs.end()) {
            return nullptr;
        }
        auto ptr = iter->second->create();
        ptr->init_with_guid(guid);
        return ptr;
    }
    BaseObject *get_object(InstanceID instance_id) const override {
        return BaseObject::get_object(instance_id);
    }
    BaseObject *get_object(vstd::Guid const &guid) const override {
        return BaseObject::get_object(guid);
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