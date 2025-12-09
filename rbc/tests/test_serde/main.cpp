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

using namespace rbc;
using namespace luisa;
int main() {
    PluginManager::init();
    {
        auto world_module = PluginManager::instance().load_module("rbc_world_v2");
        auto world_plugin = world_module->invoke<world::WorldPlugin *()>("create_world_plugin");
        auto trans = world_plugin->create_object(TypeInfo::get<world::Transform>());
        rbc::JsonSerializer writer;
        trans->rbc_objser(writer);
        auto json = writer.write_to();
        LUISA_INFO(
            "{}",
            luisa::string_view{
                (char const *)json.data(),
                json.size()});
        delete world_plugin;
    }
    rbc::JsonSerializer writer;
    StateMap state_map;
    {
        MyStruct my_struct;
        my_struct.a = 114;
        my_struct.b = make_double2(1.5, 6.6);
        my_struct.c = 514.f;
        my_struct.matrix = make_float4x4(
            1, 2, 3, 4,
            5, 6, 7, 8,
            9, 10, 11, 12,
            13, 14, 15, 16);
        my_struct.dd = "this is string";
        my_struct.ee.emplace_back(1919);
        my_struct.ee.emplace_back(810);
        my_struct.ff.try_emplace("key", make_float4(4, 3, 2, 1));
        my_struct.vec_str.emplace_back("str0");
        my_struct.vec_str.emplace_back("str1");
        my_struct.test_enum = MyEnum::Off;
        my_struct.guid.remake();
        auto &v0 = my_struct.multi_dim_vec.emplace_back();
        v0.emplace_back(1);
        v0.emplace_back(2);
        auto &v1 = my_struct.multi_dim_vec.emplace_back();
        v1.emplace_back(3);
        v1.emplace_back(4);
        state_map.write_atomic(std::move(my_struct));
    }
    // Ser
    auto text = state_map.serialize_to_json();
    auto type = TypeInfo::get<MyStruct>();
    LUISA_INFO("Type {} md5 {} json-write {}",
               type.name(),
               type.md5_to_string(),
               (char const *)text.data());

    //     // Deser
    {
        StateMap deser_map;
        deser_map.init_json({reinterpret_cast<char const *>(text.data()), text.size()});
        auto &&new_struct = deser_map.read<MyStruct>();
#define PRINT_MY_STRUCT(m) LUISA_INFO("{} {}", #m, new_struct.m)
        PRINT_MY_STRUCT(a);
        PRINT_MY_STRUCT(b);
        PRINT_MY_STRUCT(c);
        PRINT_MY_STRUCT(dd);
        PRINT_MY_STRUCT(matrix);
        for (auto &i : new_struct.ee) {
            LUISA_INFO("ee {}", i);
        }
        for (auto &i : new_struct.ff) {
            LUISA_INFO("ff {}, {}", i.first, i.second);
        }
        for (auto &i : new_struct.vec_str) {
            LUISA_INFO("vec str {}", i);
        }
        for (auto &i : new_struct.multi_dim_vec) {
            for (auto &j : i) {
                LUISA_INFO("multi_dim_vec {}", j);
            }
        }
        LUISA_INFO("test_enum {}", luisa::to_string(new_struct.test_enum));
        LUISA_INFO("guid {}", new_struct.guid.to_base64());
    }
}