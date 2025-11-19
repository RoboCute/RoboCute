import type_register as tr
from codegen_basis import *
import hashlib

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


def _print_arg_type(t, py_interface: bool = False, is_view: bool = False):
    if t == tr.string:
        if py_interface:
            return "nb::str"
        elif is_view:
            return "luisa::string_view"
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
    if type(t) is tr.struct_t:
        return t._name
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
    if type(t) is tr.struct_t:
        return t._name
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


def _print_cpp_rtti(t: tr.struct_t):
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
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]
        add_line(f"class {struct_name}:")

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
""")
    for i in extra_includes:
        add_result(i + "\n")
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]
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
            add_line('void rbc_objdeser(DeSerType& obj){')
            add_indent()
            for mem_name in struct_type._members:
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
void {export_func_name}(nanobind::module_& m)
""")
    add_result("{")
    add_indent()

    # print classes
    add_line('static char const* env = std::getenv("RBC_RUNTIME_DIR");')
    add_line(
        f'auto dynamic_module = ModuleRegister::load_module("{dll_path}");')
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]

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
            f'return dynamic_module->dll.invoke<void *()>("{create_name}");')
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
