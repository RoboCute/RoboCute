import type_register as tr
from codegen_basis import *

_type_names = {
    tr.string: "luisa::string",
    tr.byte: "int8_t",
    tr.ubyte: "uint8_t",
    tr.short: "int16_t",
    tr.ushort: "uint16_t",
    tr.int: "int32_t",
    tr.uint: "uint32_t",
    tr.long: "int64_t",
    tr.ulong: "uint64_t",
    tr.float: "float",
    tr.double: "double",
    tr.void: "void",
    tr.VoidPtr: "void*",
}


_template_names = {
    tr.vector: lambda ele: f'luisa::vector<{_print_arg_type(ele._element)}>',
    tr.unordered_map: lambda ele: f'luisa::unordered_map<{_print_arg_type(ele._key)}, {_print_arg_type(ele._value)}>',
}


def _print_arg_type(t):
    f = _type_names.get(t)
    if f:
        return f
    f = _template_names.get(type(t))
    if f:
        return f(t)
    if type(t) is tr.struct_t:
        return t._name
    if t in tr._basic_types:
        return 'luisa::' + t.__name__
    tr.log_err(f'Bad type {str(t)}')


def codegen_serde():
    set_result('''#pragma once
#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>''')
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]
        add_line(f'struct {struct_name} {{')
        add_indent()
        # members
        if len(struct_type._methods) > 0:
            tr.log_err('Serde struct can not have method')
        for mem_name in struct_type._members:
            mem = struct_type._members[mem_name]
            add_line(f'{_print_arg_type(mem)} {mem_name};')

        # serialize function
        add_line('template <typename SerType>')
        add_line('void rbc_objser(SerType& obj) const {')
        add_indent()
        for mem_name in struct_type._members:
            add_line(f'obj._store(this->{mem_name}, "{mem_name}");')
        remove_indent()
        add_line('}')

        # de-serialize function
        add_line('template <typename DeSerType>')
        add_line('void rbc_objdeser(DeSerType const& obj){')
        add_indent()
        for mem_name in struct_type._members:
            add_line(f'obj._load(this->{mem_name}, "{mem_name}");')
        remove_indent()
        add_line('}')

        remove_indent()
        add_line('};')
    return get_result()
