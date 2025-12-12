#include <rbc_world_v2/type_register.h>
#include <rbc_world_v2/transform.h>
namespace rbc::world {
static TypeRegisterBase *_type_register_header{};
static RuntimeStaticBase *_runtime_static_header{};
struct BaseObjectStatics : vstd::IOperatorNewBase {
    shared_atomic_mutex _instance_mtx;
    shared_atomic_mutex _guid_mtx;
    luisa::vector<uint64_t> _disposed_instance_ids;
    luisa::vector<BaseObject *> _instance_ids;
    luisa::unordered_map<std::array<uint64_t, 2>, BaseObject *> _obj_guids;
    luisa::unordered_map<std::array<uint64_t, 2>, TypeRegisterBase *> _create_funcs;
    BaseObjectStatics() {
        for (auto p = _type_register_header; p; p = p->p_next) {
            p->init();
            _create_funcs.try_emplace(p->type_id(), p);
        }
        for (auto p = _runtime_static_header; p; p = p->p_next) {
            p->init();
        }
    }
    ~BaseObjectStatics() {
        for (auto p = _type_register_header; p; p = p->p_next) {
            p->destroy();
        }
        for (auto p = _runtime_static_header; p; p = p->p_next) {
            p->destroy();
        }
    }
};
static BaseObjectStatics *_world_inst = nullptr;
RuntimeStaticBase::RuntimeStaticBase() {
    p_next = _runtime_static_header;
    _runtime_static_header = this;
}
RuntimeStaticBase::~RuntimeStaticBase() = default;
TypeRegisterBase::TypeRegisterBase() {
    p_next = _type_register_header;
    _type_register_header = this;
}
void _collect_all_materials();
void init_world() {
    if (_world_inst) return;
    _world_inst = new BaseObjectStatics{};
}
void destroy_world() {
    delete _world_inst;
    _world_inst = nullptr;
}
BaseObject *get_object(InstanceID instance_id) {
    BaseObject *ptr;
    std::shared_lock lck{_world_inst->_instance_mtx};
    return _world_inst->_instance_ids[instance_id._placeholder];
}
BaseObject *get_object(vstd::Guid const &guid) {
    std::shared_lock lck{_world_inst->_guid_mtx};
    auto iter = _world_inst->_obj_guids.find(reinterpret_cast<std::array<uint64_t, 2> const &>(guid));
    if (iter == _world_inst->_obj_guids.end()) {
        return nullptr;
    }
    return iter->second;
}
void BaseObject::init() {
    _guid.reset();
    std::lock_guard lck{_world_inst->_instance_mtx};
    if (_world_inst->_disposed_instance_ids.empty()) {
        _instance_id = _world_inst->_instance_ids.size();
        _world_inst->_instance_ids.push_back(static_cast<BaseObject *>(this));
    } else {
        _instance_id = _world_inst->_disposed_instance_ids.back();
        _world_inst->_disposed_instance_ids.pop_back();
        _world_inst->_instance_ids[_instance_id] = static_cast<BaseObject *>(this);
    }
}
void BaseObject::init_with_guid(vstd::Guid const &guid) {
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
    if (_instance_id != ~0ull) {
        std::lock_guard lck{_world_inst->_instance_mtx};
        _world_inst->_disposed_instance_ids.push_back(_instance_id);
        _world_inst->_instance_ids[_instance_id] = nullptr;
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
BaseObject *create_object(rbc::TypeInfo const &type_info) {
    auto iter = _world_inst->_create_funcs.find(type_info.md5());
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init();
    return ptr;
}
BaseObject *create_object_with_guid(rbc::TypeInfo const &type_info, vstd::Guid const &guid) {
    auto iter = _world_inst->_create_funcs.find(type_info.md5());
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(guid);
    return ptr;
}
BaseObject *create_object(vstd::Guid const &type_info) {
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init();
    return ptr;
}
BaseObject *create_object_with_guid(vstd::Guid const &type_info, vstd::Guid const &guid) {
    auto iter = _world_inst->_create_funcs.find(reinterpret_cast<std::array<uint64_t, 2> const &>(type_info));
    if (iter == _world_inst->_create_funcs.end()) {
        return nullptr;
    }
    auto ptr = iter->second->create();
    ptr->init_with_guid(guid);
    return ptr;
}

[[nodiscard]] luisa::span<InstanceID const> get_dirty_transforms() {
    return dirty_transforms();
}
BaseObjectType base_type_of(vstd::Guid const &type_id) {
    auto iter = _world_inst->_create_funcs.find(
        reinterpret_cast<std::array<uint64_t, 2> const &>(type_id));
    if (iter == _world_inst->_create_funcs.end())
        return BaseObjectType::Custom;
    return iter->second->base_type();
}
void clear_dirty_transform() {
    auto &v = dirty_transforms();
    for (auto &i : v) {
        auto obj = get_object(i);
        if (!obj) continue;
        LUISA_DEBUG_ASSERT(obj->is_type_of(TypeInfo::get<Transform>()));
        static_cast<Transform *>(obj)->_dirty = false;
    }
    v.clear();
}
uint64_t object_count() {
    std::shared_lock lck{_world_inst->_instance_mtx};
    return _world_inst->_instance_ids.size() - _world_inst->_disposed_instance_ids.size();
}
void dispose_all_object(vstd::Guid const &guid) {
    std::lock_guard lck{_world_inst->_guid_mtx};
    std::lock_guard lck1{_world_inst->_instance_mtx};
    for (auto &i : _world_inst->_instance_ids) {
        if (i) i->dispose();
    }
    _world_inst->_instance_ids.clear();
    _world_inst->_disposed_instance_ids.clear();
    _world_inst->_obj_guids.clear();
}
void on_before_rendering() {
    _collect_all_materials();
}
}// namespace rbc::world