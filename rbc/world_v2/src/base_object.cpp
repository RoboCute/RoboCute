#include "type_register.h"
#include <rbc_core/serde.h>
namespace rbc::world {
static std::atomic_uint64_t _instance_count{};
static luisa::spin_mutex _instance_mtx;
static luisa::unordered_map<uint64_t, BaseObject*> _instance_ids;
BaseObject* BaseObject::get_object(InstanceID instance_id) {
    BaseObject* ptr;
    _instance_mtx.lock();
    auto iter = _instance_ids.find(instance_id._placeholder);
    ptr = (iter == _instance_ids.end()) ? nullptr : iter->second;
    _instance_mtx.unlock();
    return ptr;
}
BaseObject::BaseObject() {
    _guid.reset();
    _instance_id = _instance_count++;
    _instance_mtx.lock();
    _instance_ids.try_emplace(_instance_id, this);
    _instance_mtx.unlock();
}
BaseObject::~BaseObject() {
    _instance_mtx.lock();
    _instance_ids.erase(_instance_id);
    _instance_mtx.unlock();
}
void BaseObjectImpl::rbc_objser(rbc::JsonSerializer &obj) const {
    auto tid = type_id();
    obj._store(reinterpret_cast<vstd::Guid &>(tid), "__type_id__");
    if (_guid.to_binary().data0 != 0 && _guid.to_binary().data1 != 0) {
        obj._store(_guid, "_guid");
    }
    serialize(obj);
}
void BaseObjectImpl::rbc_objdeser(rbc::JsonDeSerializer &obj) {
    if (!obj._load(_guid, "_guid")) {
        _guid.reset();
    }
    deserialize(obj);
}

}// namespace rbc::world