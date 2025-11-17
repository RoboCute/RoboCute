import type_register as tr

_line_indent = 0
_result = ""


def _add_line(line: str):
    global _result, _line_indent
    _result += '\n'
    for _ in range(_line_indent):
        _result += '\t'
    _result += line


type_names = {
    tr.string: 'luisa::string',
    tr.vector: 'std::vector',
    tr.byte: 'int8_t',
    tr.ubyte: 'uint8_t',
    tr.short: 'int16_t',
    tr.ushort: 'uint16_t',
    tr.int: 'int32_t',
    tr.uint: 'uint32_t',
    tr.long: 'int64_t',
    tr.ulong: 'uint64_t',
    tr.float: 'float',
    tr.double: 'double',
    tr.void: 'void',
    tr.ClassPtr: 'void*'
}

py_names = {
    tr.byte: 'int',
    tr.short: 'int',
    tr.int: 'int',
    tr.long: 'int',
    tr.ubyte: 'int',
    tr.ushort: 'int',
    tr.uint: 'int',
    tr.ulong: 'int',
    tr.float: 'float',
    tr.double: 'float',
    tr.string: 'str',
    tr.vector: 'list',
    tr.ClassPtr: ''
}


def print_arg_type(t, py_interface: bool, is_view: bool):
    if t == tr.string:
        if py_interface:
            return 'nb::str'
        elif is_view:
            return 'luisa::string_view'
    f = type_names.get(t)
    if f:
        return f
    if t in tr._basic_types:
        return 'luisa::' + t.__name__
    if type(t) in tr._template_types:
        f = type_names.get(type(t))
        if f:
            return f
        r = type(t).__name__ + "<"
        is_first = True
        for e in t._elements:
            if not is_first:
                r += ", "
            is_first = False
            r += print_arg_type(e, False, is_view)
        r += ">"
        return r
    if type(t) is tr.struct_t:
        return t._name
    tr.log_err(f"invalid type {str(t)}")


def print_arg_types(args: dict, py_interface: bool):
    is_first = True
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += print_arg_type(arg_type, py_interface, False)
    return r


def print_arg_vars_decl(args: dict, is_first: bool, py_interface: bool, is_view: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += print_arg_type(arg_type, py_interface, is_view)
        if not is_view:
            r += ' const& '
        else:
            r += ' '
        r += arg_name
    return r


def print_arg_vars(args: dict, is_first: bool, py_interface: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += arg_name
        if py_interface and arg_type == tr.string:
            r += '.c_str()'
    return r

def print_py_type(t):
    f = py_names.get(t)
    if f != None:
        if len(f) > 0:
            return f
        return None
    if t in tr._basic_types:
        return t.__name__
    if type(t) in tr._template_types:
        f = type_names.get(type(t))
        if f:
            return f
    if type(t) is tr.struct_t:
        return t._name
    return None

def print_py_args_decl(args: dict, is_first: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        type_str = print_py_type(arg_type)
        r += arg_name
        if type_str:
            r += ': ' + type_str
    return r


def print_py_args(args: dict, is_first : bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += arg_name
        if arg_type == tr.ClassPtr:
            r += '._handle'
    return r


def py_interface_gen(module_name: str):
    global _result, _line_indent
    _result = f'from {module_name} import *'
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]
        _add_line(f'class {struct_name}:')

        # init
        _line_indent += 1
        _add_line('def __init__(self):')
        _line_indent += 1
        _add_line(f'self._handle = create_{struct_name}()')
        _line_indent -= 1

        # dispose
        _add_line('def __del__(self):')
        _line_indent += 1
        _add_line(f'dispose_{struct_name}(self._handle)')
        _line_indent -= 1
        
        # member
        for mem_name in struct_type._member:
            mems_dict: dict = struct_type._member[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                _add_line(f'def {mem_name}(self{print_py_args_decl(func._args, False)}):')
                _line_indent += 1
                if func._ret_type:
                    func_call = 'return '
                else:
                    func_call = ''
                func_call += f'{struct_name}__{mem_name}__(self._handle{print_py_args(func._args, False)})'
                _add_line(func_call)
                _line_indent -= 1
        # nenbers
        _line_indent -= 1
    return _result


def cpp_interface_gen(*extra_includes):
    global _result, _line_indent
    _result = '''#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl/string.h>
'''
    for i in extra_includes:
        _result += i + '\n'
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]
        _add_line(f'struct {struct_name} {{')
        _result += f'''
    virtual void dispose() = 0;
    virtual ~{struct_name}() = default;'''
        _line_indent += 1

        # print members
        for mem_name in struct_type._member:
            mems_dict: dict = struct_type._member[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                if func._ret_type:
                    ret_type = print_arg_type(func._ret_type, False, False)
                else:
                    ret_type = 'void'
                _add_line(
                    f'virtual {ret_type} {mem_name}({print_arg_vars_decl(func._args, True, False, True)}) = 0;')
        _line_indent -= 1
        _add_line('};')
    return _result


def nanobind_entry_codegen(module_name: str):
    global _result, _line_indent
    _result = f'''#include "module_register.h"
namespace nb = nanobind;
using namespace nb::literals;
NB_MODULE({module_name}, m){{
    ModuleRegister::init(m);
}}
'''
    return _result


def nanobind_codegen(module_name: str, dll_path: str, *extra_includes):
    global _result, _line_indent
    _result = '''#include <nanobind/nanobind.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/logging.h>
#include "module_register.h"
'''
    export_func_name = f'export_{module_name}'
    for i in extra_includes:
        _result += i + '\n'
    _result += f'''namespace nb = nanobind;
using namespace nb::literals;
void {export_func_name}(nanobind::module_& m)
'''
    _result += '{'
    _line_indent += 1

    # print classes
    _add_line('static char const* env = std::getenv("RBC_RUNTIME_DIR");')
    _add_line(f'auto dynamic_module = ModuleRegister::load_module("{dll_path}");')
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct_t = tr._registed_struct_types[struct_name]

        # create
        create_name = f'create_{struct_name}'
        _add_line(f'm.def("create_{struct_name}", [dynamic_module]() -> void* ')
        _result += '{'
        _line_indent += 1
        _add_line('ModuleRegister::module_addref(env ? env : luisa::current_executable_path(),*dynamic_module);')
        _add_line(f'return dynamic_module->dll.invoke<void *()>("{create_name}");')
        _line_indent -= 1
        _add_line('});')
        ptr_name = f'ptr_484111b5e8ed4230b5ef5f5fdc33ca81'  # magic name
        # dispose
        _add_line(
            f'm.def("dispose_{struct_name}", [dynamic_module](void* {ptr_name})' + '{')
        _line_indent += 1
        _add_line(f'static_cast<{struct_name}*>({ptr_name})->dispose();')
        _add_line('ModuleRegister::module_deref(*dynamic_module);')
        _line_indent -= 1
        _add_line('});')

        # print members
        for mem_name in struct_type._member:
            mems_dict: dict = struct_type._member[mem_name]
            for key in mems_dict:
                ret_type = ''
                return_decl = ''
                func: tr._function_t = mems_dict[key]
                end = ''
                if func._ret_type:
                    ret_type = ' -> ' + \
                        print_arg_type(func._ret_type, True, False) + ' '
                    return_decl = 'return '
                    if func._ret_type == tr.string:
                        return_decl += "to_nb_str("
                        end = ')'

                _add_line(
                    f'm.def("{struct_name}__{mem_name}__", [](void* {ptr_name}{print_arg_vars_decl(func._args, False, True, False)}){ret_type}{{')
                _line_indent += 1
                _add_line(
                    f'{return_decl}static_cast<{struct_name}*>({ptr_name})->{mem_name}({print_arg_vars(func._args, True, True)}){end};')
                _line_indent -= 1
                _add_line('});')
    # end
    _line_indent -= 1
    _add_line('}')
    _add_line(
        f'static ModuleRegister _{export_func_name}({export_func_name});')
    return _result


