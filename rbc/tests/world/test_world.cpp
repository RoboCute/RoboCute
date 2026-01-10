#include "test_util.h"
#include <rbc_world/base_object.h>
#include <rbc_world/entity.h>
#include <rbc_world/components/transform_component.h>

TEST_SUITE("world") {
    TEST_CASE("ec") {
        using namespace rbc;
        auto entity = world::create_object<world::Entity>();
        auto transform = entity->add_component<world::TransformComponent>();
    }
}