#include <rbc_world/entity.h>
#include <rbc_world/component.h>
#include <rbc_world/type_register.h>
#include <rbc_core/runtime_static.h>

namespace rbc::world {
struct GlobalEvents {
    struct Event {
        rbc::shared_atomic_mutex mtx;
        luisa::unordered_map<vstd::Guid, coroutine> map;
    };
    std::array<Event, world_event_count> _events;
};
static RuntimeStatic<GlobalEvents> _entity_events;
void Component::_zz_invoke_world_event(WorldEventType event_type) {
    auto &evt = _entity_events->_events;
    auto &map = evt[luisa::to_underlying(event_type)];
    std::shared_lock lck{map.mtx};
    for (auto iter = map.map.begin(); iter != map.map.end();) {
        auto inst_id{iter->first};
        auto &coro = iter->second;
        if (!get_object_ref(inst_id)) {
            iter = map.map.erase(iter);
            continue;
        }
        coro.resume();
        if (coro.done()) {
            iter = map.map.erase(iter);
            continue;
        }
        ++iter;
    }
}
void Component::add_world_event(WorldEventType event_type, rbc::coroutine &&coro) {
    auto &evt = _entity_events->_events;
    auto &map = evt[luisa::to_underlying(event_type)];
    std::lock_guard lck{map.mtx};
    map.map.force_emplace(guid(), std::move(coro));
}
void Component::remove_world_event(WorldEventType event_type) {
    auto &evt = _entity_events->_events;
    auto &map = evt[luisa::to_underlying(event_type)];
    std::lock_guard lck{map.mtx};
    map.map.erase(guid());
}
Entity::Entity() {
}
Entity::~Entity() {
    for (auto &i : _components) {
        auto &comp = i.second;
        LUISA_DEBUG_ASSERT(comp->entity() == this || comp->entity() == nullptr);
        if (comp->entity())
            comp->on_destroy();
    }
    for(auto& i : _components){
        i.second->_entity = nullptr;
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

bool Entity::remove_component(MD5 const &type_md5) {
    auto iter = _components.find(type_md5);
    if (iter == _components.end()) return false;
    auto obj = std::move(iter->second);
    _components.erase(iter);
    LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
    auto comp = obj.get();
    LUISA_DEBUG_ASSERT(comp->entity() == this);
    comp->_clear_entity();
    return true;
}
Component *Entity::get_component(MD5 const &type_md5) {
    auto iter = _components.find(type_md5);
    if (iter == _components.end()) return nullptr;
    auto &obj = iter->second;
    LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
    auto comp = obj.get();
    LUISA_DEBUG_ASSERT(comp->entity() == this);
    return comp;
}
void Entity::serialize_meta(ObjSerialize const &ser) const {
    ser.ar.start_array();
    for (auto &i : _components) {
        auto comp = i.second;
        if (!comp) return;
        ser.ar.start_object();
        auto type_id = comp->type_id();
        ser.ar.value(reinterpret_cast<vstd::Guid &>(type_id), "__typeid__");
        comp->serialize_meta(ser);
        ser.ar.end_object();
    }
    ser.ar.end_array("components");
}
void Entity::deserialize_meta(ObjDeSerialize const &ser) {
    uint64_t size;
    if (!ser.ar.start_array(size, "components")) return;
    _components.reserve(size);
    for (auto &i : vstd::range(size)) {
        if (!ser.ar.start_object()) break;
        auto d = vstd::scope_exit([&] {
            ser.ar.end_scope();
        });
        vstd::Guid type_id;
        if (!ser.ar.value(type_id, "__typeid__")) {
            continue;
        }
        auto comp = _create_component(reinterpret_cast<MD5 const &>(type_id));
        if (comp) {
            comp->deserialize_meta(ser);
            auto result = _components.try_emplace(comp->type_id(), comp).second;
            LUISA_ASSERT(result, "Component already exists.");
            comp->_entity = this;
        }
    }
    ser.ar.end_scope();
}
void Entity::unsafe_call_awake() {
    for (auto &i : _components) {
        i.second->on_awake();
    }
}
void Entity::unsafe_call_update() {
    for (auto &i : _components) {
        i.second->update_data();
    }
}
void Entity::_remove_component(Component *component) {
    LUISA_DEBUG_ASSERT(component->entity() == this);
    auto iter = _components.find(component->type_id());
    LUISA_DEBUG_ASSERT(iter != _components.end());
    _components.erase(iter);
    component->_clear_entity();
}

void Component::_remove_self_from_entity() {
    if (_entity) {
        _entity->_remove_component(static_cast<Component *>(this));
    }
}

void Component::_clear_entity() {
    if (!_entity) [[unlikely]]
        return;
    on_destroy();
    _entity = nullptr;
}
Component::Component() = default;
Component::~Component() {
    int x = 0;
}
DECLARE_WORLD_OBJECT_REGISTER(Entity)
}// namespace rbc::world

// Serialize<Entity> implementation
bool rbc::Serialize<rbc::world::Entity>::write(rbc::ArchiveWrite &w, const rbc::world::Entity &v) {
    // Serialize components via serialize_meta
    rbc::world::ObjSerialize ser_obj{w};
    v.serialize_meta(ser_obj);
    return true;
}

bool rbc::Serialize<rbc::world::Entity>::read(rbc::ArchiveRead &r, rbc::world::Entity &v) {
    rbc::world::ObjDeSerialize deser_obj{r};
    v.deserialize_meta(deser_obj);
    return true;
}