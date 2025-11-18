#include "generated/generated.hpp"
#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <rbc_core/serde.h>
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
    auto text = writer.serialize("my_struct", my_struct);
    LUISA_INFO("{}", (char const *)text.data());
}