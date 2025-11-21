import rbc_meta.type_register as tr
from rbc_meta.codegen_basis import get_codegen_basis
import hashlib

cb = get_codegen_basis()

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
    tr.GUID: "GuidData",
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
    tr.vector: lambda ele: f"luisa::vector<{_print_arg_type(ele._element)}>",
    tr.unordered_map: lambda ele: f"luisa::unordered_map<{_print_arg_type(ele._key)}, {_print_arg_type(ele._value)}>",
    tr.ClassPtr: lambda ele: "void*",
    tr.external_type: lambda x:  x._name
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
    tr.unordered_map: lambda x: _is_non_serializable(x._key)
    or _is_non_serializable(x._value),
    tr.vector: lambda x: _is_non_serializable(x._element),
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


def __print_arg_vars_decl(
    args: dict, is_first: bool, py_interface: bool, is_view: bool
):
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
    if type(t) == tr.external_type:
        tr.log_err('external type not allowed in python')
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
    m = hashlib.md5(name.encode("ascii"))
    is_first = True
    digest = ''
    for i in m.digest():
        if not is_first:
            digest += ', '
        is_first = False
        digest += str(int(i))
    cb.add_result(f'''
namespace rbc_rtti_detail {{
template<>
struct is_rtti_type<{name}> {{
    static constexpr bool value = true;
    static constexpr luisa::string_view name{{"{name}"}};
    static constexpr uint8_t md5[16]{{{digest}}};
}};
}} // rbc_rtti_detail
''')


def py_interface_gen(module_name: str):
    cb.set_result(f"from {module_name} import *")
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        cb.add_line(f"class {struct_type.class_name()}:")

        # init
        cb.add_indent()
        cb.add_line("def __init__(self):")
        cb.add_indent()
        cb.add_line(f"self._handle = create__{struct_name}__()")
        cb.remove_indent()

        # dispose
        cb.add_line("def __del__(self):")
        cb.add_indent()
        cb.add_line(f"dispose__{struct_name}__(self._handle)")
        cb.remove_indent()

        # member
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                cb.add_line(
                    f"def {mem_name}(self{_print_py_args_decl(func._args, False)}):"
                )
                cb.add_indent()
                if func._ret_type:
                    func_call = "return "
                else:
                    func_call = ""
                func_call += f"{struct_name}__{mem_name}__(self._handle{_print_py_args(func._args, False)})"
                cb.add_line(func_call)
                cb.remove_indent()
        # nenbers
        cb.remove_indent()
    return cb.get_result()


def cpp_interface_gen(*extra_includes):
    cb.set_result("""#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/rtti.h>
""")
    for i in extra_includes:
        cb.add_result(i + "\n")
    # print enums
    for enum_name in tr._registed_enum_types:
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        namespace = enum_type.namespace_name()
        if len(namespace) > 0:
            cb.add_line(f"namespace {namespace} {{")
        cb.add_line(f"enum struct {enum_type.class_name()} : uint32_t {{")
        cb.add_indent()
        for param_name_and_value in enum_type._params:
            cb.add_line(f"{param_name_and_value[0]}")
            if param_name_and_value[1] != None:
                cb.add_result(f" = {param_name_and_value[1]}")
            cb.add_result(",")
        cb.remove_indent()
        cb.add_line("};")

        if len(namespace) > 0:
            cb.add_line(f"}} // namespace {namespace}")
        cb.add_result('\n')
        # enum serialize
        if enum_type._serde:
            cb.add_line(f"""template<typename SerType, typename... Args>
void rbc_objser(SerType &obj, {enum_name} const &var, Args... args) {{
    switch (var) {{""")
            cb.add_indent()
            cb.add_indent()
            enum_names = ""
            enum_values = ""
            is_first = True
            for param_name_and_value in enum_type._params:
                enun_name = param_name_and_value[0]
                cb.add_line(
                    f'case {enum_name}::{enun_name}: obj._store("{enun_name}", args...); break;'
                )
                if not is_first:
                    enum_names += ", "
                    enum_values += ", "
                is_first = False
                enum_names += f'"{enun_name}"'
                enum_values += f"luisa::to_underlying({enum_name}::{enun_name})"
            cb.remove_indent()
            cb.add_line("}")
            cb.remove_indent()
            cb.add_line("}")
            cb.add_line(f"""template<typename DeserType, typename... Args>
bool rbc_objdeser(DeserType &obj, {enum_name} &var, Args... args) {{
    luisa::string value;
    obj._load(value, args...);
    static ::rbc::EnumSerializer _ser{{std::initializer_list<char const*>{{{enum_names}}}, std::initializer_list<uint32_t>{{{enum_values}}}}};
    auto v = _ser.get_value(value.c_str());
    if (v) {{
        var = static_cast<{enum_name}>(*v);
    }}
    return v.has_value();
}}
""")

    # print classes
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        namespace = struct_type.namespace_name()
        if len(namespace) > 0:
            cb.add_line(f"namespace {namespace} {{")
        cb.add_line(
            f"struct {struct_type.class_name()} : vstd::IOperatorNewBase {{"
        )
        cb.add_indent()
        if len(struct_type._members) > 0:
            for mem_name in struct_type._members:
                mem = struct_type._members[mem_name]
                initer = struct_type._cpp_initer.get(mem_name)
                if not initer:
                    initer = ''
                cb.add_line(f"{_print_arg_type(mem)} {mem_name}{{{initer}}};")

            # serialize function
            if len(struct_type._serde_members) > 0:
                cb.add_line("template <typename SerType>")
                cb.add_line("void rbc_objser(SerType& obj) const {")
                cb.add_indent()
                for mem_name in struct_type._serde_members:
                    cb.add_line(f'obj._store(this->{mem_name}, "{mem_name}");')
                cb.remove_indent()
                cb.add_line("}")

            # de-serialize function
            if len(struct_type._serde_members) > 0:
                cb.add_line("template <typename DeSerType>")
                cb.add_line("void rbc_objdeser(DeSerType& obj){")
                cb.add_indent()
                for mem_name in struct_type._serde_members:
                    cb.add_line(f'obj._load(this->{mem_name}, "{mem_name}");')
                cb.remove_indent()
                cb.add_line("}")
        if len(struct_type._methods) > 0:
            cb.add_result(f"""
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
                cb.add_line(
                    f"virtual {ret_type} {mem_name}({__print_arg_vars_decl(func._args, True, False, True)}) = 0;"
                )
        cb.remove_indent()
        cb.add_line("};")
        if len(namespace) > 0:
            cb.add_line(f"}} // namespace {namespace}")
        cb.add_result('\n')
        # RTTI
        _print_cpp_rtti(struct_type)
    return cb.get_result()


def nanobind_codegen(module_name: str, dll_path: str, *extra_includes):
    cb.set_result("""#include <nanobind/nanobind.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/logging.h>
#include "module_register.h"
""")
    export_func_name = f"export_{module_name}"
    for i in extra_includes:
        cb.add_result(i + "\n")
    cb.add_result(f"""namespace nb = nanobind;
using namespace nb::literals;
void {export_func_name}(nanobind::module_& m) """)
    cb.add_result("{")
    cb.add_indent()

    # print enums
    for enum_name in tr._registed_enum_types:
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        cb.add_line(f'nb::enum_<{enum_name}>(m, "{enum_name}")')
        cb.add_indent()
        for params_name_and_type in enum_type._params:
            cb.add_line(
                f'.value("{params_name_and_type[0]}", {enum_name}::{params_name_and_type[0]})'
            )
        cb.remove_indent()
        cb.add_result(";")

    # print classes
    cb.add_line('static char const* env = std::getenv("RBC_RUNTIME_DIR");')
    cb.add_line(
        f'auto dynamic_module = ModuleRegister::load_module("{dll_path}");')
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]

        # create
        create_name = f"create__{struct_name}__"
        cb.add_line(f'm.def("{create_name}", [dynamic_module]() -> void* ')
        cb.add_result("{")
        cb.add_indent()
        cb.add_line("ModuleRegister::module_addref(env,*dynamic_module);")
        cb.add_line(
            f'return dynamic_module->dll.invoke<void *()>("create_{struct_name}");'
        )
        cb.remove_indent()
        cb.add_line("});")
        ptr_name = f"ptr_484111b5e8ed4230b5ef5f5fdc33ca81"  # magic name
        # dispose
        cb.add_line(
            f'm.def("dispose__{struct_name}__", [dynamic_module](void* {ptr_name})'
            + "{"
        )
        cb.add_indent()
        cb.add_line(f"static_cast<{struct_name}*>({ptr_name})->dispose();")
        cb.add_line("ModuleRegister::module_deref(*dynamic_module);")
        cb.remove_indent()
        cb.add_line("});")

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

                cb.add_line(
                    f'm.def("{struct_name}__{mem_name}__", [](void* {ptr_name}{__print_arg_vars_decl(func._args, False, True, False)}){ret_type}{{'
                )
                cb.add_indent()
                cb.add_line(
                    f"{return_decl}static_cast<{struct_name}*>({ptr_name})->{mem_name}({_print_arg_vars(func._args, True, True)}){end};"
                )
                cb.remove_indent()
                cb.add_line("});")
    # end
    cb.remove_indent()
    cb.add_line("}")
    cb.add_line(
        f"static ModuleRegister _{export_func_name}({export_func_name});")
    return cb.get_result()
