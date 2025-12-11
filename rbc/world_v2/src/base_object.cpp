#include "type_register.h"
#include <rbc_core/serde.h>
#include <rbc_world_v2/world_plugin.h>
#include <rbc_core/shared_atomic_mutex.h>
namespace rbc::world {
static shared_atomic_mutex _instance_mtx;
static shared_atomic_mutex _guid_mtx;
static luisa::vector<uint64_t> _disposed_instance_ids;
static luisa::vector<BaseObject *> _instance_ids;
static luisa::unordered_map<std::array<uint64_t, 2>, BaseObject *> _obj_guids;

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
void BaseObject::init() {
    _guid.reset();
    std::lock_guard lck{_instance_mtx};
    if (_disposed_instance_ids.empty()) {
        _instance_id = _instance_ids.size();
        _instance_ids.push_back(this);
    } else {
        _instance_id = _disposed_instance_ids.back();
        _disposed_instance_ids.pop_back();
        _instance_ids[_instance_id] = this;
    }
}
void BaseObject::init_with_guid(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(guid);
    init();
    _guid = guid;
    {
        std::lock_guard lck{_guid_mtx};
        auto new_guid = _obj_guids.try_emplace(reinterpret_cast<std::array<uint64_t, 2> const &>(guid), this).second;
        LUISA_DEBUG_ASSERT(new_guid);
    }
}
BaseObject::BaseObject() {
}
BaseObject::~BaseObject() {
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

}// namespace rbc::world