#include "type_register.h"
#include <rbc_core/serde.h>
namespace rbc::world {
BaseObject::BaseObject() {
    _guid.reset();
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
    obj._load(_guid, "_guid");
    deserialize(obj);
}

}// namespace rbc::world