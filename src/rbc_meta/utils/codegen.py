import rbc_meta.utils.type_register as tr
from rbc_meta.utils.codegen_basis import get_codegen_basis, CodeGenerator
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
        return "luisa::string"
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
    tr.external_type: lambda x: x._name,
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
    if not t:
        return "void"
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
        tr.log_err("external type not allowed in python")
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


def _print_cpp_rtti(t):
    if type(t) == tr.struct:
        name = t.full_name()
        m = hashlib.md5(name.encode("ascii"))
        is_first = True
        digest = ""
        for i in m.digest():
            if not is_first:
                digest += ", "
            is_first = False
            digest += str(int(i))
        cb.add_result(f'''
namespace rbc_rtti_detail {{
template<>
struct is_rtti_type<{name}> {{
    static constexpr bool value = true;
    static constexpr const char* name{{"{name}"}};
    static constexpr uint8_t md5[16]{{{digest}}};
}};
}} // rbc_rtti_detail
''')
    else:
        name = t.full_name()
        cb.add_result(f'''
namespace rbc_rtti_detail {{
template<>
struct is_rtti_type<{name}> {{
    static constexpr bool value = true;
    static constexpr const char* name{{"{name}"}};
}};
}} // rbc_rtti_detail
''')


def py_interface_gen(module_name: str):
    cb.set_result(f"from rbc_ext._C.{module_name} import *")
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


def _print_rpc_funcs(struct_type: tr.struct):
    global cb
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return
    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func: tr._function_t = mems_dict[key]
            is_first = True
            args = ""
            for arg_name in func._args:
                if not is_first:
                    args += ", "
                is_first = False
                args += f"{_print_arg_type(func._args[arg_name])} {arg_name}"
            static = ''
            if func._static:
                static = 'static '
            cb.add_line(
                f"{static}{_print_arg_type(func._ret_type)} {func_name}({args});")


def _print_rpc_serializer(struct_type: tr.struct):
    global cb
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return
    full_name = struct_type.full_name()
    arg_meta_defines = ""
    ret_value_defines = ""
    func_names = ""
    call_expr = ""
    is_first = True
    is_statics = ""
    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func_hasher_name = hashlib.md5(
                str(full_name + "->" + func_name + "|" + key).encode("ascii")
            ).hexdigest()
            func: tr._function_t = mems_dict[key]
            if len(func._args) > 0:
                arg_struct_name = f"Arg{func_hasher_name}"
                cb.add_line(f"struct {arg_struct_name} {{")
                cb.add_indent()
                for arg_name in func._args:
                    cb.add_line(
                        f"{_print_arg_type(func._args[arg_name])} {arg_name};")

                # serializer
                cb.add_line(
                    "void rbc_objser(rbc::JsonSerializer &obj) const {")
                cb.add_indent()
                for arg_name in func._args:
                    cb.add_line(f"obj._store({arg_name});")
                cb.remove_indent()
                cb.add_line("}")

                # deserializer
                cb.add_line("void rbc_objdeser(rbc::JsonDeSerializer &obj) {")
                cb.add_indent()
                for arg_name in func._args:
                    cb.add_line(f"obj._load({arg_name});")
                cb.remove_indent()
                cb.add_line("}")
                cb.remove_indent()
                cb.add_line("};\n")
            else:
                arg_struct_name = "void"

            if not is_first:
                func_names += ","
                call_expr += ","
                arg_meta_defines += ","
                ret_value_defines += ","
                is_statics += ", "
            is_first = False
            func_names += f'"{func_hasher_name}"'
            call_cb = CodeGenerator()
            call_cb.add_indent()
            self_name = ''
            if not func._static:
                self_name = 'void *self, '
                is_statics += "false"
            else:
                is_statics += "true"
            
            call_cb.add_line(
                f"(rbc::FuncSerializer::AnyFuncPtr)+[]({self_name}void *args, void *ret_value) {{")
            call_cb.add_indent()
            if len(func._args) > 0:
                call_cb.add_line(
                    f"auto args_ptr = static_cast<{arg_struct_name} *>(args);"
                )

            is_ret_void = not (func._ret_type and func._ret_type != tr.void)
            ret_type_name = _print_arg_type(func._ret_type)
            if not is_ret_void:
                call_cb.add_line(
                    f"std::construct_at(static_cast<{ret_type_name} *>(ret_value),"
                )
            if func._static:
                call_cb.add_line(f"{full_name}::{func_name}(")
            else:
                call_cb.add_line(f"static_cast<{full_name} *>(self)->{func_name}(")
            arg_is_first = True
            for arg_name in func._args:
                if not arg_is_first:
                    call_cb.add_result(", ")
                arg_is_first = False
                call_cb.add_result(f"args_ptr->{arg_name}")
            call_cb.add_result(")")
            if not is_ret_void:
                call_cb.add_result(")")
            call_cb.add_result(";")
            call_cb.remove_indent()
            call_cb.add_line("}")
            call_cb.remove_indent()
            call_expr += call_cb.get_result()
            arg_meta_defines += f"rbc::HeapObjectMeta::create<{arg_struct_name}>()"
            ret_value_defines += f"rbc::HeapObjectMeta::create<{ret_type_name}>()"
    hash_name = hashlib.md5(struct_type.full_name().encode('ascii')).hexdigest()
    args_defines = f"""static rbc::FuncSerializer func_ser{hash_name}{{
    std::initializer_list<const char *>{{{func_names}}},
    std::initializer_list<rbc::FuncSerializer::AnyFuncPtr>{{{call_expr}}},
    std::initializer_list<rbc::HeapObjectMeta>{{{arg_meta_defines}}},
    std::initializer_list<rbc::HeapObjectMeta>{{{ret_value_defines}}},
    std::initializer_list<bool>{{{is_statics}}}
}};"""
    cb.add_result(args_defines)


def _cpp_impl_gen():
    global cb
    # print enums
    for enum_name in tr._registed_enum_types:
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        full_name = enum_type.full_name()
        m = hashlib.md5(full_name.encode("ascii"))
        digest = m.hexdigest()
        enum_names = ""
        enum_values = ""
        is_first = True
        for param_name_and_value in enum_type._params:
            enum_value = param_name_and_value[0]
            if not is_first:
                enum_names += ", "
                enum_values += ", "
            is_first = False
            enum_names += f'"{enum_value}"'
            enum_values += f"(uint64_t)luisa::to_underlying({enum_name}::{enum_value})"
        initer = f'"{full_name}", std::initializer_list<char const*>{{{enum_names}}}, std::initializer_list<uint64_t>{{{enum_values}}}'
        cb.add_line(f"static rbc::EnumSerIniter emm_{digest}{{{initer}}};")
    # print classes
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        namespace = struct_type.namespace_name()
        if len(struct_type._serde_members) > 0:
            if len(namespace) > 0:
                cb.add_line(f"namespace {namespace} {{")
            # serialize function
            cb.add_line(
                f"void {struct_type.class_name()}::rbc_objser(rbc::JsonSerializer& obj) const {{"
            )
            cb.add_indent()
            for mem_name in struct_type._serde_members:
                cb.add_line(f"obj._store(this->{mem_name});")
            cb.remove_indent()
            cb.add_line("}")
            # de-serialize function
            cb.add_line(
                f"void {struct_type.class_name()}::rbc_objdeser(rbc::JsonDeSerializer& obj){{"
            )
            cb.add_indent()
            for mem_name in struct_type._serde_members:
                cb.add_line(f"obj._load(this->{mem_name});")
            cb.remove_indent()
            cb.add_line("}")
            if len(namespace) > 0:
                cb.add_line(f"}} // namespace {namespace}")
            cb.add_result("\n")

        _print_rpc_serializer(struct_type)


def cpp_impl_gen(*extra_includes):
    global cb
    cb.set_result("#include <rbc_core/serde.h>\n")
    for i in extra_includes:
        cb.add_result(i + "\n")
    _cpp_impl_gen()
    return cb.get_result()


JSON_SER_NAME = "b839f6ccb4b74"
SELF_NAME = "d6922fb0e4bd44549"


def _print_client_code(struct_type: tr.struct):
    global cb
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return
    cb.add_line(f"struct {struct_type.class_name()}Client {{")
    cb.add_indent()
    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func: tr._function_t = mems_dict[key]
            args = ""
            for arg_name in func._args:
                args += ", "
                args += f"{_print_arg_type(func._args[arg_name])} {arg_name}"
            future_name = "void"
            if func._ret_type and func._ret_type != tr.void:
                future_name = f"rbc::RPCFuture<{_print_arg_type(func._ret_type)}>"
            self_decl = ''
            if not func._static:
                self_decl = ', void*'
            cb.add_line(
                f"static {future_name} {func_name}(rbc::RPCCommandList &{self_decl}{args});"
            )
    cb.remove_indent()
    cb.add_line("};")


def _print_client_impl(struct_type: tr.struct):
    global cb
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return
    class_name = struct_type.class_name()
    full_name = struct_type.full_name()
    namespace = struct_type.namespace_name()
    if len(namespace) > 0:
        cb.add_line(f"namespace {namespace} {{")
    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func_hasher_name = hashlib.md5(
                str(full_name + "->" + func_name + "|" + key).encode("ascii")
            ).hexdigest()
            func: tr._function_t = mems_dict[key]
            args = ""
            for arg_name in func._args:
                args += ", "
                args += f"{_print_arg_type(func._args[arg_name])} {arg_name}"
            ret_type_name = _print_arg_type(func._ret_type)
            future_name = "void"
            if func._ret_type and func._ret_type != tr.void:
                future_name = f"rbc::RPCFuture<{ret_type_name}>"
            self_decl = ''
            self_arg = ''
            if not func._static:
                self_decl += f', void* {SELF_NAME}'
                self_arg += f', {SELF_NAME}'
            cb.add_line(
                f"{future_name} {class_name}Client::{func_name}(rbc::RPCCommandList &{JSON_SER_NAME}{self_decl}{args}){{"
            )
            cb.add_indent()
            cb.add_line(
                f'{JSON_SER_NAME}.add_functioon("{func_hasher_name}"{self_arg});'
            )
            for arg_name in func._args:
                cb.add_line(f"{JSON_SER_NAME}.add_arg({arg_name});")
            if func._ret_type and func._ret_type != tr.void:
                cb.add_line(
                    f"return {JSON_SER_NAME}.return_value<{ret_type_name}>();")

            cb.remove_indent()
            cb.add_line("}")
    if len(namespace) > 0:
        cb.add_line("}")


def cpp_client_impl_gen(*extra_includes):
    global cb
    cb.set_result("")
    for i in extra_includes:
        cb.add_result(i + "\n")
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        _print_client_impl(struct_type)
    return cb.get_result()


def cpp_client_interface_gen(*extra_includes):
    global cb
    use_rpc = False
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        if len(struct_type._rpc) > 0:
            use_rpc = True
            break
    cb.set_result("""#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/func_serializer.h>
#include <rbc_core/serde.h>""")
    if use_rpc:
        cb.add_line('#include <rbc_ipc/command_list.h>')
    for i in extra_includes:
        cb.add_result(i + "\n")
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        namespace = struct_type.namespace_name()
        if len(namespace) > 0:
            cb.add_line(f"namespace {namespace} {{")
        _print_client_code(struct_type)
        if len(namespace) > 0:
            cb.add_line(f"}} // namespace {namespace}")
    return cb.get_result()


def cpp_interface_gen(*extra_includes):
    global cb
    cb.set_result("""//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================
#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/func_serializer.h>
#include <rbc_core/serde.h>
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

        cb.add_result("\n")
        # RTTI
        _print_cpp_rtti(enum_type)

    # print classes
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        namespace = struct_type.namespace_name()
        if len(namespace) > 0:
            cb.add_line(f"namespace {namespace} {{")
        cb.add_line(
            f"struct {struct_type._suffix} {struct_type.class_name()} : vstd::IOperatorNewBase {{"
        )
        cb.add_indent()
        if struct_type._default_ctor:
            cb.add_line(f'{struct_type.class_name()}();')
        if struct_type._dtor:
            cb.add_line(f'~{struct_type.class_name()}();')
        if len(struct_type._members) > 0:
            for mem_name in struct_type._members:
                mem = struct_type._members[mem_name]
                initer = struct_type._cpp_initer.get(mem_name)
                if not initer:
                    initer = ""
                cb.add_line(f"{_print_arg_type(mem)} {mem_name}{{{initer}}};")

            # serialize function
            if len(struct_type._serde_members) > 0:
                cb.add_line("void rbc_objser(rbc::JsonSerializer& obj) const;")

            # de-serialize function
            if len(struct_type._serde_members) > 0:
                cb.add_line("void rbc_objdeser(rbc::JsonDeSerializer& obj);")
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
        # print rpc
        _print_rpc_funcs(struct_type)

        cb.remove_indent()
        cb.add_line("};")
        if len(namespace) > 0:
            cb.add_line(f"}} // namespace {namespace}")
        cb.add_result("\n")
        # RTTI
        _print_cpp_rtti(struct_type)

    cb.add_line("//! ================== GENERATED CODE END ==================")

    return cb.get_result()


def nanobind_codegen(module_name: str, dll_path: str, *extra_includes):
    cb.set_result("""#include <pybind11/pybind11.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/logging.h>
#include <rbc_core/serde.h>
#include "module_register.h"
""")
    export_func_name = f"export_{module_name}"
    for i in extra_includes:
        cb.add_result(i + "\n")
    cb.add_result(f"""namespace py = pybind11;
void {export_func_name}(py::module& m) """)
    cb.add_result("{")
    cb.add_indent()

    # print enums
    for enum_name in tr._registed_enum_types:
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        cb.add_line(f'py::enum_<{enum_name}>(m, "{enum_name}")')
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
    _cpp_impl_gen()
    return cb.get_result()
