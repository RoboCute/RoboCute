#include <rbc_world/type_register.h>
#include <rbc_world/components/transform.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/entity.h>
namespace rbc::world {
static TypeRegisterBase *_type_register_header{};
struct BaseObjectStatics : RBCStruct {
    shared_atomic_mutex _instance_mtx;
    shared_atomic_mutex _guid_mtx;
    std::atomic_uint64_t _instance_id_counter{};
    luisa::unordered_map<uint64_t, BaseObject *> _instance_ids;
    luisa::unordered_map<std::array<uint64_t, 2>, BaseObject *> _obj_guids;
    luisa::unordered_map<std::array<uint64_t, 2>, TypeRegisterBase *> _create_funcs;
    BaseObjectStatics() {
        for (auto p = _type_register_header; p; p = p->p_next) {
            p->init();
            _create_funcs.try_emplace(p->type_id(), p);
        }
    }
    ~BaseObjectStatics() {
        if (!_instance_ids.empty()) {
            LUISA_ERROR("World object is leaking.");
        }
        for (auto p = _type_register_header; p; p = p->p_next) {
            p->destroy();
        }
    }
};
static BaseObjectStatics *_world_inst = nullptr;

TypeRegisterBase::TypeRegisterBase() {
    p_next = _type_register_header;
    _type_register_header = this;
}
void _collect_all_materials();
void init_resource_loader(luisa::filesystem::path const &meta_path);// in resource_base.cpp
void dispose_resource_loader();                                     // in resource_base.cpp
void init_world(
    luisa::filesystem::path const &meta_path) {
    if (_world_inst) [[unlikely]] {
        LUISA_ERROR("World already initialized.");
    }
    _world_inst = new BaseObjectStatics{};
    if (!meta_path.empty())
        init_resource_loader(meta_path);
}
void destroy_world() {
    if (!_world_inst) [[unlikely]] {
        LUISA_ERROR("World already destroyed.");
    }
    dispose_resource_loader();
    delete _world_inst;
    _world_inst = nullptr;
}
BaseObject *get_object(InstanceID instance_id) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    if (instance_id._placeholder == ~0ull) return nullptr;
    BaseObject *ptr;
    std::shared_lock lck{_world_inst->_instance_mtx};
    auto iter = _world_inst->_instance_ids.find(instance_id._placeholder);
    if (iter != _world_inst->_instance_ids.end()) {
        return iter->second;
    }
    return nullptr;
}
BaseObject *get_object(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    std::shared_lock lck{_world_inst->_guid_mtx};
    auto iter = _world_inst->_obj_guids.find(reinterpret_cast<std::array<uint64_t, 2> const &>(guid));
    if (iter == _world_inst->_obj_guids.end()) {
        return nullptr;
    }
    return iter->second;
}
RC<BaseObject> get_object_ref(InstanceID instance_id) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    if (instance_id._placeholder == ~0ull) return {};
    BaseObject *ptr;
    std::shared_lock lck{_world_inst->_instance_mtx};
    auto iter = _world_inst->_instance_ids.find(instance_id._placeholder);
    if (iter != _world_inst->_instance_ids.end()) {
        return RC<BaseObject>{iter->second};
    }
    return {};
}
RC<BaseObject> get_object_ref(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    std::shared_lock lck{_world_inst->_guid_mtx};
    auto iter = _world_inst->_obj_guids.find(reinterpret_cast<std::array<uint64_t, 2> const &>(guid));
    if (iter == _world_inst->_obj_guids.end()) {
        return {};
    }
    return RC<BaseObject>{iter->second};
}
void BaseObject::init() {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    _guid.reset();
    _instance_id = ++_world_inst->_instance_id_counter;

    std::lock_guard lck{_world_inst->_instance_mtx};
    _world_inst->_instance_ids.force_emplace(_instance_id, this);
}
void BaseObject::init_with_guid(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    LUISA_DEBUG_ASSERT(guid);
    init();
    _guid = guid;
    {
        std::lock_guard lck{_world_inst->_guid_mtx};
        auto new_guid = _world_inst->_obj_guids.try_emplace(reinterpret_cast<std::array<uint64_t, 2> const &>(guid), static_cast<BaseObject *>(this)).second;
        LUISA_DEBUG_ASSERT(new_guid);
    }
}
BaseObject::~BaseObject() {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    if (_instance_id != ~0ull) {
        std::lock_guard lck{_world_inst->_instance_mtx};
        _world_inst->_instance_ids.erase(_instance_id);
    }
    if (_guid) {
        auto &guid = reinterpret_cast<std::array<uint64_t, 2> const &>(_guid);
        std::lock_guard lck{_world_inst->_guid_mtx};
        auto iter = _world_inst->_obj_guids.find(guid);
        LUISA_DEBUG_ASSERT(iter->second == this);
        _world_inst->_obj_guids.erase(iter);
    }
}

luisa::spin_mutex &dirty_trans_mtx();
luisa::vector<InstanceID> &dirty_transforms();
BaseObjectType get_base_object_type(vstd::Guid const &type_id) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find((std::array<uint64_t, 2> const &)type_id);
    if (iter == _world_inst->_create_funcs.end()) {
        return BaseObjectType::None;
    }
    return iter->second->base_type();
}
BaseObject *create_object(rbc::TypeInfo const &type_info) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(type_info.md5());
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(vstd::Guid(true));
    return ptr;
}
Component *Entity::_create_component(std::array<uint64_t, 2> const &type) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(type);
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create_component(this);
    ptr->init_with_guid(vstd::Guid(true));
    return ptr;
}
BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(type_info.md5());
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(guid);
    return ptr;
}
BaseObject *create_object(vstd::Guid const &type_info) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(vstd::Guid(true));
    return ptr;
}
BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(guid);
    return ptr;
}

BaseObject *_zz_create_object_with_guid_test_base(vstd::Guid const &type_info, vstd::Guid const &guid, BaseObjectType desire_type) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    if (iter->second->base_type() != desire_type) return nullptr;
    auto ptr = iter->second->create();
    ptr->init_with_guid(guid);
    return ptr;
}
BaseObjectType base_type_of(vstd::Guid const &type_id) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(
        reinterpret_cast<std::array<uint64_t, 2> const &>(type_id));
    if (iter == _world_inst->_create_funcs.end())
        return BaseObjectType::Custom;
    return iter->second->base_type();
}
void _zz_clear_dirty_transform() {
    auto &v = dirty_transforms();
    for (auto &i : v) {
        auto obj = get_object(i);
        if (!obj) continue;
        LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<TransformComponent>()));
        static_cast<TransformComponent *>(obj)->_dirty = false;
    }
    v.clear();
}
uint64_t object_count() {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    std::shared_lock lck{_world_inst->_instance_mtx};
    return _world_inst->_instance_ids.size();
}

void _zz_on_before_rendering() {
    _collect_all_materials();
    for (auto &i : dirty_transforms()) {
        auto tr_obj = get_object(i);
        if (!tr_obj || !tr_obj->is_type_of(TypeInfo::get<TransformComponent>())) {
            continue;
        }
        auto tr = static_cast<TransformComponent *>(tr_obj);
        tr->_execute_on_update_event();
    }
    _zz_clear_dirty_transform();
}
}// namespace rbc::world