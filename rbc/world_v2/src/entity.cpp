#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/component.h>
#include "type_register.h"

namespace rbc::world {
struct EntityImpl : Entity {
    ~EntityImpl() {
        for (auto &i : _components) {
            auto obj = i.second;
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
            auto comp = static_cast<Component *>(obj);
            LUISA_DEBUG_ASSERT(comp->entity() == this);
            comp->dispose();
        }
    }
    bool add_component(Component *component) override {
        auto result = _components.try_emplace(component->type_id(), component).second;
        LUISA_DEBUG_ASSERT(component->_entity == nullptr);
        component->_entity = this;
        return result;
    }
    bool remove_component(TypeInfo const &type) override {
        auto iter = _components.find(type.md5());
        if (iter == _components.end()) return false;
        auto obj = iter->second;
        _components.erase(iter);
        LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
        auto comp = static_cast<Component *>(obj);
        LUISA_DEBUG_ASSERT(comp->_entity == this);
        comp->_entity = nullptr;
        comp->dispose();
        return true;
    }
    Component *get_component(TypeInfo const &type) override {
        auto iter = _components.find(type.md5());
        if (iter == _components.end()) return nullptr;
        auto obj = iter->second;
        LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
        auto comp = static_cast<Component *>(obj);
        LUISA_DEBUG_ASSERT(comp->_entity == this);
        return comp;
    }
    void rbc_objser(rbc::JsonSerializer &ser) const override {
        ser.start_array();
        for (auto &i : _components) {
            auto comp = i.second;
            if (!comp) return;
            auto guid = comp->guid();
            if (guid) {
                ser._store(guid);
            }
        }
        ser.add_last_scope_to_object("components");
    }
    void rbc_objdeser(rbc::JsonDeSerializer &ser) override {
        uint64_t size;
        if (!ser.start_array(size, "components")) return;
        _components.reserve(size);
        for (auto &i : vstd::range(size)) {
            vstd::Guid obj_guid;
            if (!ser._load(obj_guid)) {
                break;
            }
            auto obj = get_object(obj_guid);
            LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
            add_component(static_cast<Component *>(obj));
        }
        ser.end_scope();
    }
    void dispose() override;
};
void Entity::_remove_component(Component *component) {
    LUISA_DEBUG_ASSERT(component->_entity == this);
    auto iter = _components.find(component->type_id());
    LUISA_DEBUG_ASSERT(iter != _components.end());
    _components.erase(iter);
    LUISA_DEBUG_ASSERT(component->_entity == this);
    component->_entity = nullptr;
}
void Component::_remove_self_from_entity() {
    if (_entity) {
        _entity->_remove_component(static_cast<Component*>(this));
    }
}
DECLARE_TYPE_REGISTER(Entity)
}// namespace rbc::world