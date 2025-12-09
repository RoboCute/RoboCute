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
            obj->dispose();
        }
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Entity)
}// namespace rbc::world