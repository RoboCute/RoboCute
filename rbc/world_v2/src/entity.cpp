#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/component.h>
#include "type_register.h"

namespace rbc::world {
struct EntityImpl : Entity {
    ~EntityImpl() {
        for (auto &i : _components) {
            auto obj = get_object(i.second);
            if (!obj) continue;
            LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
            auto comp = static_cast<Component *>(obj);
            LUISA_DEBUG_ASSERT(comp->entity() == this);
            comp->dispose();
        }
    }
    bool add_component(Component *component) override {
        auto result = _components.try_emplace(component->type_id(), component->instance_id()).second;
        LUISA_DEBUG_ASSERT(component->_entity == nullptr);
        component->_entity = this;
        return result;
    }
    bool remove_component(TypeInfo const &type) override {
        auto iter = _components.find(type.md5());
        if (iter == _components.end()) return false;
        auto obj = get_object(iter->second);
        _components.erase(iter);
        if (!obj) return false;
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
        auto obj = get_object(iter->second);
        if (!obj) {
            _components.erase(iter);
            return nullptr;
        }
        LUISA_DEBUG_ASSERT(obj->base_type() == BaseObjectType::Component);
        auto comp = static_cast<Component *>(obj);
        LUISA_DEBUG_ASSERT(comp->_entity == this);
        return comp;
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Entity)
}// namespace rbc::world