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

#include "generated/client.hpp"

namespace rbc {
void MyStruct::no_arg_func() {
    LUISA_INFO("calling no-arg function.");
}
double MyStruct::add(int32_t lhs, float rhs) {
    return lhs + rhs;
}
void MyStruct::add(float lhs, double rhs) {
    LUISA_INFO("void {} {}", dd, lhs + rhs);
}
}// namespace rbc
using namespace rbc;
using namespace luisa;
int main() {
    rbc::JsonSerializer writer;
    StateMap state_map;
    {
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

    // Test RPC
    JsonSerializer rpc_ser{true};// init array json
    MyStruct my_struct;
    my_struct.dd = "add_string";
    // client
    MyStructClient::no_arg_func(
        rpc_ser,
        &my_struct);
    MyStructClient::add(
        rpc_ser,
        &my_struct,
        (float)1919,
        (double)810);
    MyStructClient::add(
        rpc_ser,
        &my_struct,
        (int)114,
        (float)514);
    text = rpc_ser.write_to();
    LUISA_INFO("RPC json {},", (char const *)text.data());

    // server
    JsonSerializer rpc_ret_value_json{true};
    JsonDeSerializer rpc_deser{luisa::string_view{
        (char const *)text.data(),
        text.size()}};
    for (auto i : vstd::range(3)) {
        luisa::string func_hash;
        uint64_t self;
        LUISA_ASSERT(rpc_deser.read(func_hash));
        LUISA_ASSERT(rpc_deser.read(self));
        auto call_meta = FuncSerializer::get_call_meta(func_hash);
        LUISA_ASSERT(call_meta);
        void *arg = nullptr;
        void *ret_value = nullptr;
        // allocate
        if (call_meta->args_meta) {
            arg = call_meta->args_meta.allocate();
            call_meta->args_meta.json_reader(arg, &rpc_deser);
        }
        if (call_meta->ret_value_meta) {
            ret_value = call_meta->ret_value_meta.allocate();
        }
        // call
        call_meta->func(&my_struct, arg, ret_value);
        // deallocate
        if (call_meta->args_meta) {
            call_meta->args_meta.deallocate(arg);
        }
        if (call_meta->ret_value_meta) {
            // serialize return value
            call_meta->ret_value_meta.json_writer(ret_value, &rpc_ret_value_json);
            call_meta->ret_value_meta.deallocate(ret_value);
        }
    }
    text = rpc_ret_value_json.write_to();
    LUISA_INFO("RPC return json {},", (char const *)text.data());
}