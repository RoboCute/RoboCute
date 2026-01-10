#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <luisa/core/magic_enum.h>
#include <rbc_core/serde.h>
#include <rbc_core/type_info.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/bin_serde.h>
#include <rbc_core/state_map.h>
#include <luisa/vstl/md5.h>
#include <rbc_core/func_serializer.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_world/base_object.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/entity.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resource_base.h>
#include <rbc_core/runtime_static.h>

using namespace rbc;
using namespace luisa;

int main(int argc, char *argv[]) {
    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    auto dispose_runtime_static = vstd::scope_exit([] {
        RuntimeStaticBase::dispose_all();
    });

    world::init_world(luisa::filesystem::path{argv[0]}.parent_path());
    auto dsp_world = vstd::scope_exit([&]() {
        world::destroy_world();
    });

    // ====== Test Entity Serialization with Unified API ======
    LUISA_INFO("=== Testing Entity Serialization ===");
    luisa::BinaryBlob json;
    {
        auto entity = world::create_object<world::Entity>();
        auto saved_guid = entity->guid();

        auto trans = entity->add_component<world::TransformComponent>();
        trans->set_pos(double3(114, 514, 1919), false);

        JsonSerializer writer;
        writer._store(*entity, "entity");
        json = writer.write_to();

        auto trans_ptr = entity->get_component<world::TransformComponent>();
        // test life time
        LUISA_ASSERT(trans_ptr == trans);
        trans->delete_this();
        trans_ptr = entity->get_component<world::TransformComponent>();
        // transform already destroyed
        LUISA_ASSERT(trans_ptr == nullptr);

        // Print JSON
        auto json_str = luisa::string_view{
            (char const *)json.data(),
            json.size()};
        LUISA_INFO("Serialized Entity JSON:\n{}", json_str);

        // Try deserialize into a NEW entity (don't reuse GUID while old entity exists)
        {
            JsonDeSerializer reader{json_str};
            // Create a new entity for deserialization
            auto new_entity = world::create_object<world::Entity>();
            reader._load(*new_entity, "entity");

            auto new_trans = new_entity->get_component<world::TransformComponent>();
            if (new_trans) {
                LUISA_INFO("Deserialized position: {}", new_trans->position());
            } else {
                LUISA_WARNING("TransformComponent not found after deserialization");
            }

            // Cleanup new entity
            new_entity->delete_this();
        }

        // Cleanup original entity
        entity->delete_this();
    }

    LUISA_INFO("=== Testing Entity Serialization Done ===");

    // ====== Test Binary Serialization ======
    LUISA_INFO("=== Testing Binary Serialization ===");
    {
        auto entity = world::create_object<world::Entity>();
        auto trans = entity->add_component<world::TransformComponent>();
        trans->set_pos(double3(100, 200, 300), false);

        // Serialize with BinSerializer using _store (no name for root level)
        BinSerializer bin_writer;
        bin_writer._store(*entity, "entity");

        auto bin_blob = bin_writer.write_to();
        LUISA_INFO("Binary blob size: {} bytes", bin_blob.size());

        // Deserialize into a new entity
        {
            BinDeSerializer bin_reader{bin_blob};
            // Create a new entity for deserialization
            auto new_entity = world::create_object<world::Entity>();
            bin_reader._load(*new_entity, "entity");
            auto new_trans = new_entity->get_component<world::TransformComponent>();
            if (new_trans) {
                LUISA_INFO("Binary deserialized position: {}", new_trans->position());
            }
            new_entity->delete_this();
        }
        entity->delete_this();
    }
    LUISA_INFO("=== Testing Binary Serialization Done ===");

    return 0;
}
