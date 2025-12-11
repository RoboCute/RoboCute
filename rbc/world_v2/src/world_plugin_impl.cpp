#include <rbc_world_v2/world_plugin.h>
#include "type_register.h"
#include <rbc_world_v2/transform.h>
namespace rbc::world {
static shared_atomic_mutex _instance_mtx;
static shared_atomic_mutex _guid_mtx;
static luisa::vector<uint64_t> _disposed_instance_ids;
static luisa::vector<BaseObject *> _instance_ids;
static luisa::unordered_map<std::array<uint64_t, 2>, BaseObject *> _obj_guids;
void _collect_all_materials();
BaseObject *get_object(InstanceID instance_id) {
    BaseObject *ptr;
    std::shared_lock lck{_instance_mtx};
    return _instance_ids[instance_id._placeholder];
}
BaseObject *get_object(vstd::Guid const &guid) {
    std::shared_lock lck{_guid_mtx};
    auto iter = _obj_guids.find(reinterpret_cast<std::array<uint64_t, 2> const &>(guid));
    if (iter == _obj_guids.end()) {
        return nullptr;
    }
    return iter->second;
}
void BaseObjectBase::init() {
    _guid.reset();
    std::lock_guard lck{_instance_mtx};
    if (_disposed_instance_ids.empty()) {
        _instance_id = _instance_ids.size();
        _instance_ids.push_back(static_cast<BaseObject *>(this));
    } else {
        _instance_id = _disposed_instance_ids.back();
        _disposed_instance_ids.pop_back();
        _instance_ids[_instance_id] = static_cast<BaseObject *>(this);
    }
}
void BaseObjectBase::init_with_guid(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(guid);
    init();
    _guid = guid;
    {
        std::lock_guard lck{_guid_mtx};
        auto new_guid = _obj_guids.try_emplace(reinterpret_cast<std::array<uint64_t, 2> const &>(guid), static_cast<BaseObject *>(this)).second;
        LUISA_DEBUG_ASSERT(new_guid);
    }
}
void BaseObject::_dispose_self() {
    if (_instance_id != ~0ull) {
        std::lock_guard lck{_instance_mtx};
        _disposed_instance_ids.push_back(_instance_id);
        _instance_ids[_instance_id] = nullptr;
    }
    if (_guid) {
        auto &guid = reinterpret_cast<std::array<uint64_t, 2> const &>(_guid);
        std::lock_guard lck{_guid_mtx};
        auto iter = _obj_guids.find(guid);
        LUISA_DEBUG_ASSERT(iter->second == this);
        _obj_guids.erase(iter);
    }
}
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
        return rbc::world::get_object(instance_id);
    }
    BaseObject *get_object(vstd::Guid const &guid) const override {
        return rbc::world::get_object(guid);
    }
    [[nodiscard]] luisa::span<InstanceID const> get_dirty_transforms() const override {
        return dirty_transforms();
    }
    BaseObjectType base_type_of(vstd::Guid const &type_id) const override {
        auto iter = _create_funcs.find(
            reinterpret_cast<std::array<uint64_t, 2> const &>(type_id));
        if (iter == _create_funcs.end())
            return BaseObjectType::Custom;
        return iter->second->base_obj_type;
    }
    void clear_dirty_transform() override {
        auto &v = dirty_transforms();
        for (auto &i : v) {
            auto obj = get_object(i);
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<Transform>()));
            static_cast<Transform *>(obj)->_dirty = false;
        }
        v.clear();
    }
    uint64_t object_count() const override {
        std::shared_lock lck{_instance_mtx};
        return _instance_ids.size() - _disposed_instance_ids.size();
    }
    void dispose_all_object(vstd::Guid const &guid) override {
        std::lock_guard lck{_guid_mtx};
        std::lock_guard lck1{_instance_mtx};
        for (auto &i : _instance_ids) {
            if (i) i->dispose();
        }
        _instance_ids.clear();
        _disposed_instance_ids.clear();
        _obj_guids.clear();
    }
    void on_before_rendering() override {
        _collect_all_materials();
    }
};
LUISA_EXPORT_API WorldPlugin *get_world_plugin() {
    static WorldPluginImpl world_plugin_singleton{};
    return &world_plugin_singleton;
}
}// namespace rbc::world