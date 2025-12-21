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
#include <rbc_world_v2/base_object.h>
#include <rbc_world_v2/transform.h>
#include <rbc_world_v2/entity.h>
#include <rbc_world_v2/resources/material.h>
#include <rbc_core/runtime_static.h>

using namespace rbc;
using namespace luisa;
int main() {
    RuntimeStaticBase::init_all();
    auto dispose_runtime_static = vstd::scope_exit([] {
        RuntimeStaticBase::dispose_all();
    });
    world::init_world();
    auto dsp_world = vstd::scope_exit([&]() {
        world::destroy_world();
    });
    luisa::BinaryBlob json;
    {
        auto entity = world::create_object_with_guid<world::Entity>(vstd::Guid(true));

        auto trans = entity->add_component<world::Transform>();
        trans->set_pos(double3(114, 514, 1919), false);
        JsonSerializer writer(true);
        world::ObjSerialize writer_args(writer);
        // serialize entity and components
        auto guid = entity->guid();
        auto type_id = entity->type_id();
        writer.start_object();
        if (guid) {
            writer._store(guid, "__guid__");
            entity->serialize_meta(writer_args);
        }
        writer.add_last_scope_to_object();
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
    auto json_str = luisa::string_view{
        (char const *)json.data(),
        json.size()};
    LUISA_INFO("{}", json_str);
    // // {
    rbc::JsonDeSerializer reader{json_str};
    auto entity_size = reader.last_array_size();
    luisa::vector<world::Entity *> entities;
    entities.reserve(entity_size);
    for (auto i : vstd::range(entity_size)) {
        reader.start_object();
        vstd::Guid guid;
        LUISA_ASSERT(reader._load(guid, "__guid__"));
        auto entity = entities.emplace_back(world::create_object_with_guid<world::Entity>(guid));
        auto deser_obj = world::ObjDeSerialize{.ser = reader};
        entity->deserialize_meta(deser_obj);
        reader.end_scope();
    }
    for (auto &i : entities) {
        auto trans = i->get_component<world::Transform>();
        LUISA_INFO("{}", trans->position());
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