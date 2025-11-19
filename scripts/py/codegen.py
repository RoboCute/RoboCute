import type_register as tr
from codegen_basis import *
import hashlib

_type_names = {
    tr.bool: "bool",
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
    tr.GUID: "GuidData"
}


def _print_str(t, py_interface: bool = False, is_view: bool = False):
    if py_interface:
        return "nb::str"
    elif is_view:
        return "luisa::string_view"
    else:
        return "luisa::string"


def _print_guid(t, py_interface: bool = False, is_view: bool = False):
    if py_interface:
        return "GuidData"
    elif is_view:
        return "vstd::Guid const&"
    else:
        return "vstd::Guid"


_type_name_functions = {
    tr.string: _print_str,
    tr.GUID: _print_guid,
}


_template_names = {
    tr.vector: lambda ele: f'luisa::vector<{_print_arg_type(ele._element)}>',
    tr.unordered_map: lambda ele: f'luisa::unordered_map<{_print_arg_type(ele._key)}, {_print_arg_type(ele._value)}>',
    tr.ClassPtr: lambda ele: 'void*'
}

_py_names = {
    tr.byte: "int",
    tr.short: "int",
    tr.int: "int",
    tr.long: "int",
    tr.ubyte: "int",
    tr.ushort: "int",
    tr.uint: "int",
    tr.ulong: "int",
    tr.float: "float",
    tr.double: "float",
    tr.string: "str",
    tr.vector: "list",
    tr.VoidPtr: "",
}

_serde_blacklist = {
    tr.VoidPtr: lambda x: True,
    tr.ClassPtr: lambda x: True,
    tr.unordered_map: lambda x:  _is_non_serializable(
        x._key) or _is_non_serializable(x._value),
    tr.vector: lambda x: _is_non_serializable(x._element)
}


def _is_non_serializable(x):
    v = _serde_blacklist.get(x)
    if v:
        return v(x)
    v = _serde_blacklist.get(type(x))
    if v:
        return v(x)


def _print_arg_type(t, py_interface: bool = False, is_view: bool = False):
    f = _type_name_functions.get(t)
    if f:
        return f(t, py_interface, is_view)
    f = _type_names.get(t)
    if f:
        return f
    if t in tr._basic_types:
        return "luisa::" + t.__name__
    if type(t) in tr._template_types:
        f = _template_names.get(type(t))
        if f:
            return f(t)
        tr.log_err(f"invalid type {str(t)}")
    if type(t) is tr.struct or type(t) is tr.enum:
        return t.full_name()
    tr.log_err(f"invalid type {str(t)}")


def __print_arg_vars_decl(args: dict, is_first: bool, py_interface: bool, is_view: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += _print_arg_type(arg_type, py_interface, is_view)
        if not is_view:
            r += " const& "
        else:
            r += " "
        r += arg_name
    return r


def _print_arg_vars(args: dict, is_first: bool, py_interface: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += arg_name
        if py_interface and arg_type == tr.string:
            r += ".c_str()"
    return r


def _print_py_type(t):
    f = _py_names.get(t)
    if f != None:
        if len(f) > 0:
            return f
        return None
    if t in tr._basic_types:
        return t.__name__
    if type(t) is tr.struct or type(t) is tr.enum:
        return t.class_name()
    return None


def _print_py_args_decl(args: dict, is_first: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        type_str = _print_py_type(arg_type)
        r += arg_name
        if type_str:
            r += ": " + type_str
    return r


def _print_py_args(args: dict, is_first: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += arg_name
        if arg_type == tr.VoidPtr or type(arg_type) == tr.ClassPtr:
            r += "._handle"
    return r


def _print_cpp_rtti(t: tr.struct):
    name = t.full_name()
    add_line(
        f'static constexpr luisa::string_view _zz_typename_ {{"{name}"}};')
    m = hashlib.md5(name.encode('ascii'))
    hex = m.hexdigest()
    add_line(
        f'static constexpr uint64_t _zz_md5_[2] {{{int(hex[0:16], 16)}, {int(hex[16:32], 16)} }};')


def py_interface_gen(module_name: str):
    set_result(f"from {module_name} import *")
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        add_line(f"class {struct_type.class_name()}:")

        # init
        add_indent()
        add_line("def __init__(self):")
        add_indent()
        add_line(f"self._handle = create__{struct_name}__()")
        remove_indent()

        # dispose
        add_line("def __del__(self):")
        add_indent()
        add_line(f"dispose__{struct_name}__(self._handle)")
        remove_indent()

        # member
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                add_line(
                    f"def {mem_name}(self{_print_py_args_decl(func._args, False)}):"
                )
                add_indent()
                if func._ret_type:
                    func_call = "return "
                else:
                    func_call = ""
                func_call += f"{struct_name}__{mem_name}__(self._handle{_print_py_args(func._args, False)})"
                add_line(func_call)
                remove_indent()
        # nenbers
        remove_indent()
    return get_result()


def cpp_interface_gen(*extra_includes):
    set_result("""#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/enum_serializer.h>
""")
    for i in extra_includes:
        add_result(i + "\n")
    # print enums
    for enum_name in tr._registed_enum_types:
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        namespace = enum_type.namespace_name()
        if len(namespace) > 0:
            add_line(f'namespace {namespace} {{')
        add_line(f'enum struct {enum_type.class_name()} : uint32_t {{')
        add_indent()
        for param_name_and_value in enum_type._params:
            add_line(f'{param_name_and_value[0]}')
            if param_name_and_value[1] != None:
                add_result(f' = {param_name_and_value[1]}')
            add_result(',')
        remove_indent()
        add_line('};')

        if len(namespace) > 0:
            add_line('}')
        # enum serialize
        if enum_type._serde:
            add_line(f'''template<typename SerType, typename... Args>
void rbc_objser(SerType &obj, {enum_name} const &var, Args... args) {{
    switch (var) {{''')
            add_indent()
            add_indent()
            enum_names = ""
            enum_values = ""
            is_first = True
            for param_name_and_value in enum_type._params:
                enun_name = param_name_and_value[0]
                add_line(
                    f'case {enum_name}::{enun_name}: obj._store("{enun_name}", args...); break;')
                if not is_first:
                    enum_names += ', '
                    enum_values += ', '
                is_first = False
                enum_names += f'"{enun_name}"'
                enum_values += f'luisa::to_underlying({enum_name}::{enun_name})'
            remove_indent()
            add_line('}')
            remove_indent()
            add_line('}')
            add_line(f'''template<typename DeserType, typename... Args>
bool rbc_objdeser(DeserType &obj, {enum_name} &var, Args... args) {{
    luisa::string value;
    obj._load(value, args...);
    static ::rbc::EnumSerializer _ser{{std::initializer_list<char const*>{{{enum_names}}}, std::initializer_list<uint32_t>{{{enum_values}}}}};
    auto v = _ser.get_value(value.c_str());
    if (v) {{
        var = static_cast<{enum_name}>(*v);
    }}
    return v.has_value();
}}''')

    # print classes
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        namespace = struct_type.namespace_name()
        if (len(namespace) > 0):
            add_line(f'namespace {namespace} {{')
        add_line(
            f"struct {struct_type.class_name()} : public vstd::IOperatorNewBase {{")
        # RTTI
        add_indent()
        _print_cpp_rtti(struct_type)
        if len(struct_type._members) > 0:
            for mem_name in struct_type._members:
                mem = struct_type._members[mem_name]
                default_val = struct_type._default_value.get(mem_name)
                # TODO: default value
                # if (default_val)
                add_line(f'{_print_arg_type(mem)} {mem_name}{{}};')

            # serialize function
            if len(struct_type._serde_members) > 0:
                add_line('template <typename SerType>')
                add_line('void rbc_objser(SerType& obj) const {')
                add_indent()
                for mem_name in struct_type._serde_members:
                    add_line(f'obj._store(this->{mem_name}, "{mem_name}");')
                remove_indent()
                add_line('}')

            # de-serialize function
            if len(struct_type._serde_members) > 0:
                add_line('template <typename DeSerType>')
                add_line('void rbc_objdeser(DeSerType& obj){')
                add_indent()
                for mem_name in struct_type._serde_members:
                    add_line(f'obj._load(this->{mem_name}, "{mem_name}");')
                remove_indent()
                add_line('}')
        if len(struct_type._methods) > 0:
            add_result(f"""
    virtual void dispose() = 0;
    virtual ~{struct_name}() = default;""")

        # print methods
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                if func._ret_type:
                    ret_type = _print_arg_type(func._ret_type, False, False)
                else:
                    ret_type = "void"
                add_line(
                    f"virtual {ret_type} {mem_name}({__print_arg_vars_decl(func._args, True, False, True)}) = 0;"
                )
        remove_indent()
        add_line("};")
        if (len(namespace) > 0):
            add_line('}')
    return get_result()


def nanobind_codegen(module_name: str, dll_path: str, *extra_includes):

    set_result("""#include <nanobind/nanobind.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/logging.h>
#include "module_register.h"
""")
    export_func_name = f"export_{module_name}"
    for i in extra_includes:
        add_result(i + "\n")
    add_result(f"""namespace nb = nanobind;
using namespace nb::literals;
void {export_func_name}(nanobind::module_& m) """)
    add_result("{")
    add_indent()

    # print enums
    for enum_name in tr._registed_enum_types:
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        add_line(f'nb::enum_<{enum_name}>(m, "{enum_name}")')
        add_indent()
        for params_name_and_type in enum_type._params:
            add_line(
                f'.value("{params_name_and_type[0]}", {enum_name}::{params_name_and_type[0]})')
        remove_indent()
        add_result(';')

    # print classes
    add_line('static char const* env = std::getenv("RBC_RUNTIME_DIR");')
    add_line(
        f'auto dynamic_module = ModuleRegister::load_module("{dll_path}");')
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]

        # create
        create_name = f"create__{struct_name}__"
        add_line(
            f'm.def("{create_name}", [dynamic_module]() -> void* ')
        add_result("{")
        add_indent()
        add_line(
            "ModuleRegister::module_addref(env,*dynamic_module);"
        )
        add_line(
            f'return dynamic_module->dll.invoke<void *()>("create_{struct_name}");')
        remove_indent()
        add_line("});")
        ptr_name = f"ptr_484111b5e8ed4230b5ef5f5fdc33ca81"  # magic name
        # dispose
        add_line(
            f'm.def("dispose__{struct_name}__", [dynamic_module](void* {ptr_name})' + "{"
        )
        add_indent()
        add_line(f"static_cast<{struct_name}*>({ptr_name})->dispose();")
        add_line("ModuleRegister::module_deref(*dynamic_module);")
        remove_indent()
        add_line("});")

        # print members
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                ret_type = ""
                return_decl = ""
                func: tr._function_t = mems_dict[key]
                end = ""
                if func._ret_type:
                    ret_type = (
                        " -> " +
                        _print_arg_type(func._ret_type, True, False) + " "
                    )
                    return_decl = "return "
                    if func._ret_type == tr.string:
                        return_decl += "to_nb_str("
                        end = ")"

                add_line(
                    f'm.def("{struct_name}__{mem_name}__", [](void* {ptr_name}{__print_arg_vars_decl(func._args, False, True, False)}){ret_type}{{'
                )
                add_indent()
                add_line(
                    f"{return_decl}static_cast<{struct_name}*>({ptr_name})->{mem_name}({_print_arg_vars(func._args, True, True)}){end};"
                )
                remove_indent()
                add_line("});")
    # end
    remove_indent()
    add_line("}")
    add_line(
        f"static ModuleRegister _{export_func_name}({export_func_name});")
    return get_result()
