import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen as codegen
import rbc_meta.utils.codegen_util as ut
from pathlib import Path


def codegen_header(header_path: Path):
    MyStruct = tr.struct("rbc::MyStruct")
    MyEnum = tr.enum("rbc::MyEnum", On=1, Off=None)
    MyStruct.serde_members(
        guid=tr.GUID,
        multi_dim_vec=tr.external_type("luisa::vector<luisa::vector<int32_t>>"),
        matrix=tr.float4x4,
        a=tr.int,
        b=tr.double2,
        c=tr.float,
        dd=tr.string,
        ee=tr.external_type("luisa::vector<int32_t>"),
        ff=tr.unordered_map(tr.string, tr.float4),
        vec_str=tr.external_type("luisa::vector<luisa::string>"),
        test_enum=MyEnum,
    )
    ut.codegen_to(header_path)(codegen.cpp_interface_gen)

    include = f'#include "{header_path.name}"'
    ut.codegen_to(header_path.parent / "enum_ser.cpp")(codegen.cpp_impl_gen, include)
    
    client_path = header_path.parent / 'client.hpp'
    ut.codegen_to(client_path)(codegen.cpp_client_interface_gen)
    include = '#include "client.hpp"'
    client_path = header_path.parent / 'client.cpp'
    ut.codegen_to(client_path)(codegen.cpp_client_impl_gen, include)
