#include "test_util.h"
#include <rbc_world_v2/base_object.h>
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/components/transform.h>

TEST_SUITE("world") {
    TEST_CASE("ec") {
        using namespace rbc;
        auto entity = world::create_object<world::Entity>();
        auto transform = entity->add_component<world::TransformComponent>();
    }
}