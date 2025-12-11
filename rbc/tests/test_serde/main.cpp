#include "generated/generated.hpp"
#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <luisa/core/magic_enum.h>
#include <rbc_core/serde.h>
#include <rbc_core/type_info.h>
#include <rbc_core/json_serde.h>
#include <rbc_core/state_map.h>
#include <luisa/vstl/md5.h>
#include <rbc_core/func_serializer.h>
#include <rbc_runtime/plugin_manager.h>
#include <rbc_world_v2/world_plugin.h>
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/material.h>

using namespace rbc;
using namespace luisa;
int main() {
    PluginManager::init();

    auto world_module = PluginManager::instance().load_module("rbc_world_v2");
    auto world_plugin = world_module->invoke<world::WorldPlugin *()>("get_world_plugin");
    luisa::BinaryBlob json;
    {
        auto entity = world_plugin->create_object_with_guid<world::Entity>(vstd::Guid(true));
        auto trans = world_plugin->create_object_with_guid<world::Transform>(vstd::Guid(true));
        entity->add_component(trans);
        trans->set_pos(double3(114, 514, 1919), false);
        rbc::JsonSerializer writer;
        // serialize entity and components
        luisa::vector<std::pair<vstd::Guid, vstd::Guid>> type_meta;
        auto serialize_obj = [&](world::BaseObject *obj) {
            auto guid = obj->guid();
            auto type_id = obj->type_id();
            if (guid) {
                writer.start_object();
                obj->rbc_objser(writer);
                writer.add_last_scope_to_object(guid.to_base64().c_str());
                type_meta.emplace_back(reinterpret_cast<vstd::Guid &>(type_id), guid);
            }
        };
        {
            for (auto i : *entity) {
                serialize_obj(i);
            }
            serialize_obj(entity);
        }
        writer.start_array();
        for (auto &i : type_meta) {
            writer._store(i.first);
            writer._store(i.second);
        }
        writer.add_last_scope_to_object("meta");
        json = writer.write_to();
        auto trans_ptr = entity->get_component(TypeInfo::get<world::Transform>());
        // test life time
        LUISA_ASSERT(trans_ptr == trans);
        trans->dispose();
        trans_ptr = entity->get_component(TypeInfo::get<world::Transform>());
        // tarnsform already destroyed
        LUISA_ASSERT(trans_ptr == nullptr);
        entity->dispose();
    }
    // Try deserialize
    {
        auto json_str = luisa::string_view{
            (char const *)json.data(),
            json.size()};
        LUISA_INFO("{}", json_str);
        rbc::JsonDeSerializer reader{json_str};
        uint64_t meta_array_size;
        LUISA_ASSERT(reader.start_array(meta_array_size, "meta"));
        LUISA_ASSERT(meta_array_size % 2 == 0);
        luisa::vector<world::BaseObject *> objs;
        // create objects
        for (auto i : vstd::range(meta_array_size / 2)) {
            vstd::Guid type_id, guid;
            LUISA_ASSERT(reader._load(type_id));
            LUISA_ASSERT(reader._load(guid));
            auto obj = world_plugin->create_object_with_guid(type_id, guid);
            LUISA_ASSERT(obj);
            objs.emplace_back(obj);
        }
        reader.end_scope();
        // deserialize content
        world::Entity *entity{};
        world::Transform *transform{};
        for (auto &obj : objs) {
            auto guid_str = obj->guid().to_base64();
            LUISA_ASSERT(reader.start_object(guid_str.c_str()));
            obj->rbc_objdeser(reader);
            reader.end_scope();
            if (obj->base_type() == world::BaseObjectType::Entity) {
                entity = static_cast<world::Entity *>(obj);
            } else {
                transform = static_cast<world::Transform *>(obj);
            }
        }
        LUISA_ASSERT(entity->component_count() == 1);
        LUISA_INFO("{}", transform->position());
    }
    return 0;
    //     rbc::JsonSerializer writer;
    //     StateMap state_map;
    //     {
    //         MyStruct my_struct;
    //         my_struct.a = 114;
    //         my_struct.b = make_double2(1.5, 6.6);
    //         my_struct.c = 514.f;
    //         my_struct.matrix = make_float4x4(
    //             1, 2, 3, 4,
    //             5, 6, 7, 8,
    //             9, 10, 11, 12,
    //             13, 14, 15, 16);
    //         my_struct.dd = "this is string";
    //         my_struct.ee.emplace_back(1919);
    //         my_struct.ee.emplace_back(810);
    //         my_struct.ff.try_emplace("key", make_float4(4, 3, 2, 1));
    //         my_struct.vec_str.emplace_back("str0");
    //         my_struct.vec_str.emplace_back("str1");
    //         my_struct.test_enum = MyEnum::Off;
    //         my_struct.guid.remake();
    //         auto &v0 = my_struct.multi_dim_vec.emplace_back();
    //         v0.emplace_back(1);
    //         v0.emplace_back(2);
    //         auto &v1 = my_struct.multi_dim_vec.emplace_back();
    //         v1.emplace_back(3);
    //         v1.emplace_back(4);
    //         state_map.write_atomic(std::move(my_struct));
    //     }
    //     // Ser
    //     auto text = state_map.serialize_to_json();
    //     auto type = TypeInfo::get<MyStruct>();
    //     LUISA_INFO("Type {} md5 {} json-write {}",
    //                type.name(),
    //                type.md5_to_string(),
    //                (char const *)text.data());

    //     //     // Deser
    //     {
    //         StateMap deser_map;
    //         deser_map.init_json({reinterpret_cast<char const *>(text.data()), text.size()});
    //         auto &&new_struct = deser_map.read<MyStruct>();
    // #define PRINT_MY_STRUCT(m) LUISA_INFO("{} {}", #m, new_struct.m)
    //         PRINT_MY_STRUCT(a);
    //         PRINT_MY_STRUCT(b);
    //         PRINT_MY_STRUCT(c);
    //         PRINT_MY_STRUCT(dd);
    //         PRINT_MY_STRUCT(matrix);
    //         for (auto &i : new_struct.ee) {
    //             LUISA_INFO("ee {}", i);
    //         }
    //         for (auto &i : new_struct.ff) {
    //             LUISA_INFO("ff {}, {}", i.first, i.second);
    //         }
    //         for (auto &i : new_struct.vec_str) {
    //             LUISA_INFO("vec str {}", i);
    //         }
    //         for (auto &i : new_struct.multi_dim_vec) {
    //             for (auto &j : i) {
    //                 LUISA_INFO("multi_dim_vec {}", j);
    //             }
    //         }
    //         LUISA_INFO("test_enum {}", luisa::to_string(new_struct.test_enum));
    //         LUISA_INFO("guid {}", new_struct.guid.to_base64());
    //     }
}