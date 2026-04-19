#include <rbc_world/entity.h>
#include <rbc_world/component.h>
#include <rbc_world/type_register.h>
#include <rbc_world/resources/scene.h>
#include <rbc_core/runtime_static.h>

namespace rbc::world {
struct GlobalEvents {
    struct Event {
        rbc::shared_atomic_mutex mtx;
        luisa::unordered_map<Component *, std::pair<coroutine, RCWeak<Component>>> map;
    };
    std::array<Event, world_event_count> _events;
};
static RuntimeStatic<GlobalEvents> _entity_events;
void dispose_entity_events() {
    if (!_entity_events) [[unlikely]]
        return;
    for (auto &i : _entity_events->_events) {
        std::lock_guard lck{i.mtx};
        i.map.clear();
    }
}
void Entity::remove_self_from_scene() {
    if (_parent_scene) {
        _parent_scene->remove_entity(this->guid());
    }
}
void Component::_zz_invoke_world_event(WorldEventType event_type) {
    auto &evt = _entity_events->_events;
    auto &map = evt[luisa::to_underlying(event_type)];
    std::shared_lock lck{map.mtx};
    for (auto iter = map.map.begin(); iter != map.map.end();) {
        [[maybe_unused]] auto inst_id{iter->first};
        auto &coro = iter->second.first;
        auto obj = iter->second.second.lock().rc();
        if (!obj) {
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
    map.map.force_emplace(this, std::move(coro), RCWeak<Component>{this});
}
void Component::remove_world_event(WorldEventType event_type) {
    auto &evt = _entity_events->_events;
    auto &map = evt[luisa::to_underlying(event_type)];
    std::lock_guard lck{map.mtx};
    map.map.erase(this);
}
Entity::Entity() = default;

Entity::~Entity() {
    for (auto &i : _components) {
        auto &comp = i.second;
        LUISA_DEBUG_ASSERT(comp->entity() == this || comp->entity() == nullptr);
        comp->_call_on_destroy();
    }
    for (auto &i : _components) {
        i.second->_entity = nullptr;
    }
}
void Component::_call_on_awake() {
    if (!_enabled.exchange(true)) [[likely]]
        on_awake();
}
void Component::_call_on_destroy() {
    if (_enabled.exchange(false)) [[likely]]
        on_destroy();
}

bool Entity::remove_component(MD5 const &type_md5) {
    Component *comp;
    {
        std::lock_guard lck{_add_comp_mtx};
        auto iter = _components.find(type_md5);
        if (iter == _components.end()) return false;
        auto obj = std::move(iter->second);
        _components.erase(iter);
        LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
        comp = obj.get();
        LUISA_DEBUG_ASSERT(comp->entity() == this);
    }
    comp->_clear_entity();
    return true;
}
Component *Entity::_get_or_add_component(vstd::MD5 type_id, luisa::move_only_function<RC<Component>()> const &create_func) {
    Component *p;
    bool new_value;
    {
        std::lock_guard lck{_add_comp_mtx};
        auto iter = _components.try_emplace(type_id, vstd::lazy_eval(create_func));
        p = iter.first->second.get();
        new_value = iter.second;
    }
    if (new_value) {
        p->remove_self_from_entity();
        p->_entity = this;
        p->_call_on_awake();
    }
    return p;
}

Component *Entity::get_component(MD5 const &type_md5) {
    std::shared_lock lck{_add_comp_mtx};
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
        auto const &comp = i.second;
        if (!comp) return;
        ser.ar.start_object();
        auto type_id = comp->type_id();
        ser.ar.value(reinterpret_cast<vstd::Guid &>(type_id), "__typeid__");
        comp->serialize_meta(ser);
        ser.ar.end_object();
    }
    ser.ar.end_array("components");
    if (!_name.empty())
        ser.ar.add(_name, "name");
}
void Entity::deserialize_meta(ObjDeSerialize const &ser) {
    uint64_t size;
    if (!ser.ar.read(_name, "name")) {
        _name.clear();
    }

    if (!ser.ar.start_array(size, "components")) return;
    _components.reserve(size);
    for ([[maybe_unused]] auto &i : vstd::range(static_cast<int64_t>(size))) {
        if (!ser.ar.start_object()) break;
        auto d = vstd::scope_exit([&] {
            ser.ar.end_scope();
        });
        vstd::Guid type_id{};
        if (!ser.ar.value(type_id, "__typeid__")) {
            continue;
        }
        auto comp = _create_component(reinterpret_cast<MD5 const &>(type_id));
        if (comp) {
            comp->deserialize_meta(ser);
            auto result = _components.try_emplace(comp->type_id(), comp).second;
            if (!result) [[unlikely]]
                LUISA_ERROR("Component already exists.");
            comp->_entity = this;
        }
    }
    ser.ar.end_scope();
}
void Entity::unsafe_call_awake() {
    for (auto &i : _components) {
        i.second->_call_on_awake();
    }
}
void Entity::unsafe_call_update() {
    for (auto &i : _components) {
        i.second->update_data();
    }
}
void Entity::_remove_component(Component *component) {

    {
        std::lock_guard lck{_add_comp_mtx};
        LUISA_DEBUG_ASSERT(component->entity() == this);
        auto iter = _components.find(component->type_id());
        LUISA_DEBUG_ASSERT(iter != _components.end());
        _components.erase(iter);
    }
    component->_clear_entity();
}

void Component::remove_self_from_entity() {
    auto e = _entity.exchange(nullptr);
    if (!e) [[unlikely]]
        return;
    e->_remove_component(static_cast<Component *>(this));
}

void Component::_clear_entity() {
    auto e = _entity.exchange(nullptr);
    if (!e) [[unlikely]]
        return;
    _call_on_destroy();
}
void Entity::set_name(luisa::string name) {
    if (_parent_scene) {
        _parent_scene->_set_entity_name(this, name);
    }
    _name = std::move(name);
}
Component::Component() = default;
Component::~Component() = default;
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