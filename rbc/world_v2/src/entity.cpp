#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/component.h>
#include <rbc_world_v2/type_register.h>

namespace rbc::world {

Entity::~Entity() {
    for (auto &i : _components) {
        auto obj = i.second;
        LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
        auto comp = static_cast<Component *>(obj);
        LUISA_DEBUG_ASSERT(comp->entity() == this);
        comp->dispose();
    }
}
void Entity::_add_component(Component *component) {
    component->_remove_self_from_entity();
    auto result = _components.try_emplace(component->type_id(), component).second;
    LUISA_ASSERT(result, "Component already exists.");
    LUISA_DEBUG_ASSERT(component->entity() == nullptr);
    component->_entity = this;
    component->on_awake();
}

bool Entity::remove_component(TypeInfo const &type) {
    auto iter = _components.find(type.md5());
    if (iter == _components.end()) return false;
    auto obj = iter->second;
    _components.erase(iter);
    LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
    auto comp = static_cast<Component *>(obj);
    LUISA_DEBUG_ASSERT(comp->entity() == this);
    comp->_clear_entity();
    for (auto &i : _events) {
        i.second.erase(comp);
    }
    comp->dispose();
    return true;
}
Component *Entity::get_component(TypeInfo const &type) {
    auto iter = _components.find(type.md5());
    if (iter == _components.end()) return nullptr;
    auto obj = iter->second;
    LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
    auto comp = static_cast<Component *>(obj);
    LUISA_DEBUG_ASSERT(comp->entity() == this);
    return comp;
}
void Entity::rbc_objser(rbc::JsonSerializer &ser) const {
    ser.start_array();
    for (auto &i : _components) {
        auto comp = i.second;
        if (!comp) return;
        ser.start_object();
        auto type_id = comp->type_id();
        ser._store(
            reinterpret_cast<vstd::Guid &>(type_id),
            "__typeid__");
        comp->rbc_objser(ser);
        ser.add_last_scope_to_object();
    }
    ser.add_last_scope_to_object("components");
}
void Entity::rbc_objdeser(rbc::JsonDeSerializer &ser) {
    uint64_t size;
    if (!ser.start_array(size, "components")) return;
    _components.reserve(size);
    for (auto &i : vstd::range(size)) {
        if (!ser.start_object()) break;
        auto d = vstd::scope_exit([&] {
            ser.end_scope();
        });
        vstd::Guid type_id;
        if (!ser._load(type_id, "__typeid__")) {
            continue;
        }
        auto comp = _create_component(reinterpret_cast<std::array<uint64_t, 2> const &>(type_id));
        _add_component(comp);
        comp->rbc_objdeser(ser);
    }
    ser.end_scope();
}
void Entity::_remove_component(Component *component) {
    LUISA_DEBUG_ASSERT(component->entity() == this);
    auto iter = _components.find(component->type_id());
    LUISA_DEBUG_ASSERT(iter != _components.end());
    _components.erase(iter);
    component->_clear_entity();
    for (auto &i : _events) {
        i.second.erase(component);
    }
}
void Component::add_event(WorldEventType event, void (Component::*func_ptr)()) {
    auto iter = entity()->_events.try_emplace(event);
    iter.first->second.try_emplace(this, func_ptr);
}

void Entity::broadcast_event(WorldEventType frame_tick) {
    auto iter = _events.find(frame_tick);
    if (iter == _events.end()) {
        return;
    }
    for (auto &i : iter->second) {
        (i.first->*i.second)();
    }
}
void Component::_remove_self_from_entity() {
    if (_entity) {
        _entity->_remove_component(static_cast<Component *>(this));
    }
}
void Component::remove_event(WorldEventType event) {
    auto iter = _entity->_events.find(event);
    if (iter == _entity->_events.end()) return;
    iter->second.erase(this);
}
void Component::_clear_entity() {
    if (!_entity) [[unlikely]]
        return;
    on_destroy();
    _entity = nullptr;
}
Component::Component(Entity *entity) {}
Component::~Component() {}
DECLARE_WORLD_OBJECT_REGISTER(Entity)
}// namespace rbc::world