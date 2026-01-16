#include <rbc_world/type_register.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/entity.h>
namespace rbc::world {
static TypeRegisterBase *_type_register_header{};
struct BaseObjectStatics : RBCStruct {
    shared_atomic_mutex _guid_mtx;
    luisa::unordered_map<MD5, RCWeak<BaseObject>> _obj_guids;
    luisa::unordered_map<MD5, TypeRegisterBase *> _create_funcs;
    void init_register(TypeRegisterBase *p) {
        p->init();
        _create_funcs.try_emplace(p->type_id(), p);
    }
    BaseObjectStatics() {
        for (auto p = _type_register_header; p; p = p->p_next) {
            init_register(p);
        }
    }
    ~BaseObjectStatics() {
        // collect dangling objects
        luisa::vector<RC<BaseObject>> remove_obj;
        std::lock_guard lck{_guid_mtx};
        if (!-_obj_guids.empty()) {
            for (auto iter = _obj_guids.begin(); iter != _obj_guids.end();) {
                auto o = iter->second.lock().rc();
                if (o->rbc_rc_count() > 0) {
                    ++iter;
                    continue;
                }
                o->_guid.reset();
                remove_obj.emplace_back(std::move(o));
                iter = _obj_guids.erase(iter);
            }
            remove_obj.clear();
        }
        if (!_obj_guids.empty()) {
            for(auto& i : _obj_guids){
                auto ptr = i.second.lock().rc();
                if(ptr) {
                    LUISA_INFO("{} leaking with rc {}", (size_t)ptr.get(), ptr->rbc_rc_count());
                }
            }
            LUISA_ERROR("World object is leaking.");
        }
        for (auto p = _type_register_header; p; p = p->p_next) {
            p->destroy();
        }
    }
};
static BaseObjectStatics *_world_inst = nullptr;
void TypeRegisterBase::_base_init() {
    if (_world_inst) {
        _world_inst->init_register(this);
    } else {
        p_next = _type_register_header;
        _type_register_header = this;
    }
}
void _collect_all_materials();
void init_resource_loader(luisa::filesystem::path const &meta_path, luisa::filesystem::path const &binary_path);// in resource_base.cpp
void dispose_resource_loader();                                                                                 // in resource_base.cpp
void init_world(
    luisa::filesystem::path const &meta_path,
    luisa::filesystem::path const &binary_path) {
    if (_world_inst) [[unlikely]] {
        LUISA_ERROR("World already initialized.");
    }
    _world_inst = new BaseObjectStatics{};
    if (!meta_path.empty())
        init_resource_loader(meta_path, binary_path.empty() ? meta_path : binary_path);
}
void destroy_world() {
    if (!_world_inst) [[unlikely]] {
        return;
    }
    dispose_resource_loader();
    delete _world_inst;
    _world_inst = nullptr;
}
void get_all_objects(vstd::function<void(BaseObject *)> const &callback) {
    std::shared_lock lck{_world_inst->_guid_mtx};
    for (auto &i : _world_inst->_obj_guids) {
        auto v = i.second.lock().rc();
        if (v)
            callback(v.get());
    }
}
RC<BaseObject> get_object_ref(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    std::shared_lock lck{_world_inst->_guid_mtx};
    auto iter = _world_inst->_obj_guids.find(reinterpret_cast<MD5 const &>(guid));
    if (iter == _world_inst->_obj_guids.end()) {
        return {};
    }
    return iter->second.lock().rc();
}
void BaseObject::init() {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    _guid.reset();
}
void BaseObject::init_with_guid(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    LUISA_DEBUG_ASSERT(guid);
    init();
    _guid = guid;
    {
        std::lock_guard lck{_world_inst->_guid_mtx};
        auto new_guid = _world_inst->_obj_guids.try_emplace(reinterpret_cast<MD5 const &>(guid), static_cast<BaseObject *>(this)).second;
        if (!new_guid) [[unlikely]] {
            LUISA_ERROR("Creating object with non-unique GUID {}", guid.to_string());
        }
    }
}
BaseObject::~BaseObject() {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    if (_guid) {
        std::lock_guard lck{_world_inst->_guid_mtx};
        _world_inst->_obj_guids.erase(_guid);
    }
}

luisa::spin_mutex &dirty_trans_mtx();
luisa::vector<RCWeak<TransformComponent>> &dirty_transforms();
BaseObjectType get_base_object_type(vstd::Guid const &type_id) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find((MD5 const &)type_id);
    if (iter == _world_inst->_create_funcs.end()) {
        return BaseObjectType::None;
    }
    return iter->second->base_type();
}
BaseObject *create_object(rbc::TypeInfo const &type_info) {
    return create_object(type_info.md5());
}

Component *Entity::_create_component(MD5 const &type) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(type);
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(vstd::Guid(true));
    return static_cast<Component *>(ptr);
}
BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid) {
    return create_object_with_guid(type_info.md5(), guid);
}
BaseObject *create_object(vstd::Guid const &type_info) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<MD5 const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(vstd::Guid(true));
    return ptr;
}
BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<MD5 const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(guid);
    return ptr;
}

BaseObject *_zz_create_object_with_guid_test_base(vstd::Guid const &type_info, vstd::Guid const &guid, BaseObjectType desire_type) {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<MD5 const &>(type_info));
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
        reinterpret_cast<MD5 const &>(type_id));
    if (iter == _world_inst->_create_funcs.end())
        return BaseObjectType::Custom;
    return iter->second->base_type();
}
bool world_transform_dirty() {
    auto &v = dirty_transforms();
    return !v.empty();
}
void _zz_clear_dirty_transform() {
    auto &v = dirty_transforms();
    for (auto &i : v) {
        auto obj = i.lock().rc();
        if (!obj) continue;
        LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<TransformComponent>()));
        static_cast<TransformComponent *>(obj.get())->_dirty = false;
    }
    v.clear();
}
uint64_t object_count() {
    LUISA_DEBUG_ASSERT(_world_inst, "World already destroyed.");
    std::shared_lock lck{_world_inst->_guid_mtx};
    return _world_inst->_obj_guids.size();
}

void _zz_on_before_rendering() {
    _collect_all_materials();
    for (auto &i : dirty_transforms()) {
        auto tr_obj = i.lock().rc();
        if (!tr_obj || !tr_obj->is_type_of(TypeInfo::get<TransformComponent>())) {
            continue;
        }
        auto tr = static_cast<TransformComponent *>(tr_obj.get());
        tr->_execute_on_update_event();
    }
    _zz_clear_dirty_transform();
}
}// namespace rbc::world