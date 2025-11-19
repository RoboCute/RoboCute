#include "rbc_core/state_map.hpp"
namespace rbc {
void StateMap::copy_to(StateMap &dst) {
    // TODO

}

void StateMap::save() const {
    // TODO

}
void StateMap::clear() {
    // TODO

}
void StateMap::release() {
    // TODO

}
auto StateMap::HeapObject::duplicate() const -> HeapObject {
    // TODO
    return *this;
}
void StateMap::HeapObject::move(const HeapObject &rhs) {
    // TODO

}
void StateMap::HeapObject::dtor() {
    // TODO

}
void StateMap::HeapObject::dtor_and_free() {
    // TODO

}
StateMap::~StateMap() {
    // TODO

}
}// namespace rbc