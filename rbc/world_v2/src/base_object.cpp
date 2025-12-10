#include "type_register.h"
#include <rbc_core/serde.h>
namespace rbc::world {
static std::atomic_uint64_t _instance_count{};
static luisa::spin_mutex _instance_mtx;
static luisa::unordered_map<uint64_t, BaseObject *> _instance_ids;
static luisa::unordered_map<std::array<uint64_t, 2>, BaseObject *> _obj_guids;

BaseObject *get_object(InstanceID instance_id) {
    BaseObject *ptr;
    _instance_mtx.lock();
    auto iter = _instance_ids.find(instance_id._placeholder);
    ptr = (iter == _instance_ids.end()) ? nullptr : iter->second;
    _instance_mtx.unlock();
    return ptr;
}
BaseObject *get_object(vstd::Guid const &guid) {
    BaseObject *ptr;
    _instance_mtx.lock();
    auto iter = _obj_guids.find(reinterpret_cast<std::array<uint64_t, 2> const&>(guid));
    ptr = (iter == _obj_guids.end()) ? nullptr : iter->second;
    _instance_mtx.unlock();
    return ptr;
}
void BaseObject::init() {
    _guid.reset();
    _instance_id = _instance_count++;
    _instance_mtx.lock();
    _instance_ids.try_emplace(_instance_id, this);
    _instance_mtx.unlock();
}
void BaseObject::init_with_guid(vstd::Guid const &guid) {
    LUISA_DEBUG_ASSERT(guid);
    _guid = guid;
    _instance_id = _instance_count++;
    std::lock_guard lck{_instance_mtx};
    _instance_ids.try_emplace(_instance_id, this);
    auto new_guid = _obj_guids.try_emplace(reinterpret_cast<std::array<uint64_t, 2> const &>(guid), this).second;
    LUISA_DEBUG_ASSERT(new_guid);
}
BaseObject::BaseObject() {
}
BaseObject::~BaseObject() {
    if (_instance_id != ~0ull) {
        std::lock_guard lck{_instance_mtx};
        _instance_ids.erase(_instance_id);
    }
    if (_guid) {
        auto &guid = reinterpret_cast<std::array<uint64_t, 2> const &>(_guid);
        std::lock_guard lck{_instance_mtx};
        auto iter = _obj_guids.find(guid);
        LUISA_DEBUG_ASSERT(iter->second == this);
        _obj_guids.erase(iter);
    }
}

}// namespace rbc::world