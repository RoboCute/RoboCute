#include "generated/generated.hpp"
#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <luisa/core/magic_enum.h>
#include <rbc_core/serde.h>
#include <rbc_core/type.h>
#include <rbc_core/json_serde.h>
using namespace rbc;
using namespace luisa;

int main() {
    rbc::Serializer<rbc::JsonWriter> writer;
    MyStruct my_struct;
    my_struct.a = 114;
    my_struct.b = make_double2(1.5, 6.6);
    my_struct.c = 514.f;
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
    // Ser
    auto text = writer.serialize("my_struct", my_struct);
    auto type = Type::get<MyStruct>();
    LUISA_INFO("Type {} md5 {} json-write {}",
               type.name(),
               type.md5_to_string(),
               (char const *)text.data());

    // Deser
    MyStruct new_struct;
    rbc::DeSerializer<rbc::JsonReader> reader(luisa::string_view{(char const *)text.data(), text.size()});
    reader._load(new_struct, "my_struct");
#define PRINT_MY_STRUCT(m) LUISA_INFO("{} {}", #m, new_struct.m)
    PRINT_MY_STRUCT(a);
    PRINT_MY_STRUCT(b);
    PRINT_MY_STRUCT(c);
    PRINT_MY_STRUCT(dd);
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