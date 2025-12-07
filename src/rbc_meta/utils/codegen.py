import rbc_meta.utils.type_register as tr
from rbc_meta.utils.codegen_basis import get_codegen_basis
import hashlib

from rbc_meta.utils_next.templates import (
    DEFAULT_INDENT,
    CPP_ENUM_KVPAIR_TEMPLATE,
    CPP_ENUM_TEMPLATE,
    CPP_STRUCT_TEMPLATE,
    CPP_STRUCT_MEMBER_EXPR_TEMPLATE,
    CPP_STRUCT_SER_DECL_TEMPLATE,
    CPP_STRUCT_DESER_DECL_TEMPLATE,
    CPP_STRUCT_METHOD_DECL_TEMPLATE,
    CPP_INTERFACE_TEMPLATE,
    PY_INTERFACE_CLASS_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE,
    PY_DISPOSE_METHOD_TEMPLATE,
    PY_METHOD_TEMPLATE,
    CPP_IMPL_TEMPLATE,
    CPP_ENUM_INITER_TEMPLATE,
    CPP_STRUCT_SER_IMPL_TEMPLATE,
    CPP_STRUCT_DESER_IMPL_TEMPLATE,
    CPP_RPC_ARG_STRUCT_TEMPLATE,
    CPP_RPC_ARG_MEMBER_TEMPLATE,
    CPP_RPC_SER_STMT_TEMPLATE,
    CPP_RPC_DESER_STMT_TEMPLATE,
    CPP_RPC_CALL_LAMBDA_TEMPLATE,
    CPP_FUNC_SERIALIZER_TEMPLATE,
    CPP_CLIENT_INTERFACE_TEMPLATE,
    CPP_CLIENT_CLASS_TEMPLATE,
    CPP_CLIENT_METHOD_DECL_TEMPLATE,
    CPP_CLIENT_IMPL_TEMPLATE,
    CPP_CLIENT_METHOD_IMPL_TEMPLATE,
    CPP_CLIENT_ADD_ARG_STMT_TEMPLATE,
    CPP_CLIENT_RETURN_STMT_TEMPLATE,
    PYBIND_CODE_TEMPLATE,
    PYBIND_ENUM_BINDING_TEMPLATE,
    PYBIND_ENUM_VALUE_TEMPLATE,
    PYBIND_CREATE_FUNC_TEMPLATE,
    PYBIND_DISPOSE_FUNC_TEMPLATE,
    PYBIND_METHOD_FUNC_TEMPLATE,
)

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

pybind_argument_pass = {tr.DataBuffer: lambda x: "to_span_5d4636ab"}
pybind_return_pass = {tr.DataBuffer: lambda x: "to_memoryview_5d4636ab"}


def _print_str(t, py_interface: bool = False, is_view: bool = False):
    if is_view:
        return "luisa::string_view"
    elif py_interface:
        return "luisa::string"
    else:
        return "luisa::string"


def _print_guid(t, py_interface: bool = False, is_view: bool = False):
    if py_interface:
        return "GuidData"
    elif is_view:
        return "vstd::Guid const&"
    else:
        return "vstd::Guid"


def _print_data_buffer(t, py_interface: bool = False, is_view: bool = False):
    if py_interface:
        if is_view:
            return "py::buffer const&"
        else:
            return "py::memoryview"
    else:
        return "luisa::span<std::byte>"


_type_name_functions = {
    tr.string: _print_str,
    tr.GUID: _print_guid,
    tr.DataBuffer: _print_data_buffer,
}


_template_names = {
    tr.unordered_map: lambda ele: f"luisa::unordered_map<{get_arg_type(ele._key)}, {get_arg_type(ele._value)}>",
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
    tr.VoidPtr: "",
    tr.DataBuffer: "",
}

_serde_blacklist = {
    tr.VoidPtr: lambda x: True,
    tr.ClassPtr: lambda x: True,
    tr.unordered_map: lambda x: _is_non_serializable(x._key)
    or _is_non_serializable(x._value),
}


def _is_non_serializable(x):
    v = _serde_blacklist.get(x)
    if v:
        return v(x)
    v = _serde_blacklist.get(type(x))
    if v:
        return v(x)


def get_arg_type(t, py_interface: bool = False, is_view: bool = False):
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


def _print_arg_vars_decl(args: dict, is_first: bool, py_interface: bool, is_view: bool):
    r = ""
    for arg_name in args:
        arg_type = args[arg_name]
        if not is_first:
            r += ", "
        is_first = False
        r += get_arg_type(arg_type, py_interface, is_view)
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
        f = pybind_argument_pass.get(arg_type)
        if f:
            r += f(arg_type) + "("
        r += arg_name
        if f:
            r += ")"
    return r


def _print_py_type(t):
    f = _py_names.get(t)
    if f is not None:
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
        if isinstance(arg_type, tr.ClassPtr):
            r += "._handle"
    return r


def py_interface_gen(module_name: str):
    INDENT = DEFAULT_INDENT

    def get_class_expr(struct_name: str):
        struct_type: tr.struct = tr._registed_struct_types[struct_name]

        init_method = PY_INIT_METHOD_TEMPLATE.substitute(
            INDENT=INDENT,
            STRUCT_NAME=struct_name,
        )

        dispose_method = PY_DISPOSE_METHOD_TEMPLATE.substitute(
            INDENT=INDENT,
            STRUCT_NAME=struct_name,
        )

        def get_method_expr(mem_name: str, func: tr._function_t):
            args_decl = _print_py_args_decl(func._args, False)
            args_call = _print_py_args(func._args, False)
            return_expr = "return " if func._ret_type else ""

            return PY_METHOD_TEMPLATE.substitute(
                INDENT=INDENT,
                METHOD_NAME=mem_name,
                ARGS_DECL=args_decl,
                RETURN_EXPR=return_expr,
                STRUCT_NAME=struct_name,
                ARGS_CALL=args_call,
            )

        methods_list = []
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                methods_list.append(get_method_expr(mem_name, func))

        methods_expr = "".join(methods_list)

        return PY_INTERFACE_CLASS_TEMPLATE.substitute(
            CLASS_NAME=struct_type.class_name(),
            INIT_METHOD=init_method,
            DISPOSE_METHOD=dispose_method,
            METHODS_EXPR=methods_expr,
        )

    classes_expr = "\n".join(
        [get_class_expr(struct_name) for struct_name in tr._registed_struct_types]
    )

    result = f"from rbc_ext._C.{module_name} import *\n{classes_expr}"
    cb.set_result(result)
    return cb.get_result()


def _print_rpc_serializer(struct_type: tr.struct):
    INDENT = DEFAULT_INDENT
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return ""
    full_name = struct_type.full_name()

    arg_structs = []
    func_names_list = []
    call_exprs_list = []
    arg_metas_list = []
    ret_metas_list = []
    is_statics_list = []

    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func_hasher_name = hashlib.md5(
                str(full_name + "->" + func_name + "|" + key).encode("ascii")
            ).hexdigest()
            func: tr._function_t = mems_dict[key]

            arg_struct_name = "void"
            if len(func._args) > 0:
                arg_struct_name = f"Arg{func_hasher_name}"

                arg_members = "\n".join(
                    [
                        CPP_RPC_ARG_MEMBER_TEMPLATE.substitute(
                            INDENT=INDENT,
                            ARG_TYPE=get_arg_type(func._args[arg_name]),
                            ARG_NAME=arg_name,
                        )
                        for arg_name in func._args
                    ]
                )

                ser_stmts = "\n".join(
                    [
                        CPP_RPC_SER_STMT_TEMPLATE.substitute(
                            INDENT=INDENT,
                            ARG_NAME=arg_name,
                        )
                        for arg_name in func._args
                    ]
                )

                deser_stmts = "\n".join(
                    [
                        CPP_RPC_DESER_STMT_TEMPLATE.substitute(
                            INDENT=INDENT,
                            ARG_NAME=arg_name,
                        )
                        for arg_name in func._args
                    ]
                )

                arg_struct = CPP_RPC_ARG_STRUCT_TEMPLATE.substitute(
                    ARG_STRUCT_NAME=arg_struct_name,
                    ARG_MEMBERS=arg_members,
                    INDENT=INDENT,
                    SER_STMTS=ser_stmts,
                    DESER_STMTS=deser_stmts,
                )
                arg_structs.append(arg_struct)

            func_names_list.append(f'"{func_hasher_name}"')

            # Build lambda call expression
            self_param = "void *self, " if not func._static else ""
            is_statics_list.append("true" if func._static else "false")

            args_cast = ""
            if len(func._args) > 0:
                args_cast = f"{INDENT}{INDENT}auto args_ptr = static_cast<{arg_struct_name} *>(args);\n"

            is_ret_void = not (func._ret_type and func._ret_type != tr.void)
            ret_type_name = get_arg_type(func._ret_type)

            ret_construct = ""
            if not is_ret_void:
                ret_construct = f"{INDENT}{INDENT}std::construct_at(static_cast<{ret_type_name} *>(ret_value),\n"

            func_call_prefix = (
                f"{full_name}::{func_name}("
                if func._static
                else f"static_cast<{full_name} *>(self)->{func_name}("
            )
            args_call = (
                ", ".join([f"args_ptr->{arg_name}" for arg_name in func._args])
                if len(func._args) > 0
                else ""
            )
            func_call_suffix = ")" if not is_ret_void else ""
            func_call = (
                f"{INDENT}{INDENT}{func_call_prefix}{args_call}){func_call_suffix};"
            )

            call_expr = CPP_RPC_CALL_LAMBDA_TEMPLATE.substitute(
                SELF_PARAM=self_param,
                ARGS_CAST=args_cast,
                RET_CONSTRUCT=ret_construct,
                FUNC_CALL=func_call,
                RET_CLOSE="",
            )
            call_exprs_list.append(call_expr)

            arg_metas_list.append(f"rbc::HeapObjectMeta::create<{arg_struct_name}>()")
            ret_metas_list.append(f"rbc::HeapObjectMeta::create<{ret_type_name}>()")

    hash_name = hashlib.md5(struct_type.full_name().encode("ascii")).hexdigest()

    result_parts = []
    result_parts.extend(arg_structs)

    func_serializer = CPP_FUNC_SERIALIZER_TEMPLATE.substitute(
        HASH_NAME=hash_name,
        FUNC_NAMES=", ".join(func_names_list),
        CALL_EXPRS=", ".join(call_exprs_list),
        ARG_METAS=", ".join(arg_metas_list),
        RET_METAS=", ".join(ret_metas_list),
        IS_STATICS=", ".join(is_statics_list),
    )
    result_parts.append(func_serializer)

    return "\n".join(result_parts)


def _cpp_impl_gen():
    INDENT = DEFAULT_INDENT

    def get_enum_initer_expr(enum_name: str):
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        if enum_type._cpp_external:
            return ""

        full_name = enum_type.full_name()
        m = hashlib.md5(full_name.encode("ascii"))
        digest = m.hexdigest()

        enum_names = ", ".join([f'"{kv[0]}"' for kv in enum_type._params])
        enum_values = ", ".join(
            [
                f"(uint64_t)luisa::to_underlying({enum_name}::{kv[0]})"
                for kv in enum_type._params
            ]
        )

        initer = f'"{full_name}", std::initializer_list<char const*>{{{enum_names}}}, std::initializer_list<uint64_t>{{{enum_values}}}'

        return CPP_ENUM_INITER_TEMPLATE.substitute(
            DIGEST=digest,
            INITER=initer,
        )

    enum_initers = [
        get_enum_initer_expr(enum_name) for enum_name in tr._registed_enum_types
    ]
    enum_initers_expr = "\n".join([e for e in enum_initers if e])

    def get_struct_impl_expr(struct_name: str):
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        if struct_type._cpp_external:
            return ""

        namespace = struct_type.namespace_name()
        namespace_open = f"namespace {namespace} {{\n" if len(namespace) > 0 else ""
        namespace_close = f"\n}} // namespace {namespace}" if len(namespace) > 0 else ""

        result_parts = []

        if len(struct_type._serde_members) > 0:
            store_stmts = "\n".join(
                [
                    f"{INDENT}{INDENT}obj._store(this->{mem_name});"
                    for mem_name in struct_type._serde_members
                ]
            )

            ser_impl = CPP_STRUCT_SER_IMPL_TEMPLATE.substitute(
                NAMESPACE_OPEN=namespace_open,
                CLASS_NAME=struct_type.class_name(),
                STORE_STMTS=store_stmts,
            )
            result_parts.append(ser_impl)

            load_stmts = "\n".join(
                [
                    f"{INDENT}{INDENT}obj._load(this->{mem_name});"
                    for mem_name in struct_type._serde_members
                ]
            )

            deser_impl = CPP_STRUCT_DESER_IMPL_TEMPLATE.substitute(
                CLASS_NAME=struct_type.class_name(),
                LOAD_STMTS=load_stmts,
                NAMESPACE_CLOSE=namespace_close,
            )
            result_parts.append(deser_impl)

        rpc_serializer = _print_rpc_serializer(struct_type)
        if rpc_serializer:
            result_parts.append(rpc_serializer)

        return "\n".join(result_parts)

    struct_impls = [
        get_struct_impl_expr(struct_name) for struct_name in tr._registed_struct_types
    ]
    struct_impls_expr = "\n".join([s for s in struct_impls if s])

    return enum_initers_expr, struct_impls_expr


def cpp_impl_gen(*extra_includes):
    extra_includes_expr = "\n".join(extra_includes) if extra_includes else ""

    enum_initers_expr, struct_impls_expr = _cpp_impl_gen()

    result = CPP_IMPL_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        ENUM_INITERS_EXPR=enum_initers_expr,
        STRUCT_IMPLS_EXPR=struct_impls_expr,
    )

    cb.set_result(result)
    return cb.get_result()


JSON_SER_NAME = "b839f6ccb4b74"
SELF_NAME = "d6922fb0e4bd44549"


def _print_client_code(struct_type: tr.struct):
    INDENT = DEFAULT_INDENT
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return ""

    def get_method_decl(func_name: str, func: tr._function_t):
        args_decl = ", ".join(
            [
                f"{get_arg_type(func._args[arg_name])} {arg_name}"
                for arg_name in func._args
            ]
        )
        if args_decl:
            args_decl = ", " + args_decl

        ret_type = "void"
        if func._ret_type and func._ret_type != tr.void:
            ret_type = f"rbc::RPCFuture<{get_arg_type(func._ret_type)}>"

        self_param = ", void*" if not func._static else ""

        return CPP_CLIENT_METHOD_DECL_TEMPLATE.substitute(
            INDENT=INDENT,
            RET_TYPE=ret_type,
            METHOD_NAME=func_name,
            SELF_PARAM=self_param,
            ARGS_DECL=args_decl,
        )

    method_decls = []
    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func: tr._function_t = mems_dict[key]
            method_decls.append(get_method_decl(func_name, func))

    methods_decl = "\n".join(method_decls)

    namespace = struct_type.namespace_name()
    namespace_open = f"namespace {namespace} {{\n" if len(namespace) > 0 else ""
    namespace_close = f"\n}} // namespace {namespace}" if len(namespace) > 0 else ""

    return CPP_CLIENT_CLASS_TEMPLATE.substitute(
        NAMESPACE_OPEN=namespace_open,
        CLASS_NAME=struct_type.class_name(),
        METHOD_DECLS=methods_decl,
        NAMESPACE_CLOSE=namespace_close,
    )


def _print_client_impl(struct_type: tr.struct):
    INDENT = DEFAULT_INDENT
    rpc = struct_type._rpc
    if len(rpc) == 0:
        return ""

    class_name = struct_type.class_name()
    full_name = struct_type.full_name()
    namespace = struct_type.namespace_name()
    namespace_open = f"namespace {namespace} {{\n" if len(namespace) > 0 else ""
    namespace_close = f"\n}} // namespace {namespace}" if len(namespace) > 0 else ""

    method_impls = []
    is_first = True

    for func_name in rpc:
        mems_dict: dict = rpc[func_name]
        for key in mems_dict:
            func: tr._function_t = mems_dict[key]
            func_hasher_name = hashlib.md5(
                str(full_name + "->" + func_name + "|" + key).encode("ascii")
            ).hexdigest()

            args_decl = ", ".join(
                [
                    f"{get_arg_type(func._args[arg_name])} {arg_name}"
                    for arg_name in func._args
                ]
            )
            if args_decl:
                args_decl = ", " + args_decl

            ret_type_name = get_arg_type(func._ret_type)
            ret_type = "void"
            if func._ret_type and func._ret_type != tr.void:
                ret_type = f"rbc::RPCFuture<{ret_type_name}>"

            self_param = f", void* {SELF_NAME}" if not func._static else ""
            self_arg = f", {SELF_NAME}" if not func._static else ""

            add_args_stmts = "\n".join(
                [
                    CPP_CLIENT_ADD_ARG_STMT_TEMPLATE.substitute(
                        INDENT=INDENT,
                        JSON_SER_NAME=JSON_SER_NAME,
                        ARG_NAME=arg_name,
                    )
                    for arg_name in func._args
                ]
            )

            return_stmt = ""
            if func._ret_type and func._ret_type != tr.void:
                return_stmt = CPP_CLIENT_RETURN_STMT_TEMPLATE.substitute(
                    INDENT=INDENT,
                    JSON_SER_NAME=JSON_SER_NAME,
                    RET_TYPE=ret_type_name,
                )

            method_impl = CPP_CLIENT_METHOD_IMPL_TEMPLATE.substitute(
                NAMESPACE_OPEN=namespace_open if is_first else "",
                RET_TYPE=ret_type,
                CLASS_NAME=class_name,
                METHOD_NAME=func_name,
                JSON_SER_NAME=JSON_SER_NAME,
                SELF_PARAM=self_param,
                ARGS_DECL=args_decl,
                FUNC_HASH=func_hasher_name,
                SELF_ARG=self_arg,
                ADD_ARGS_STMTS=add_args_stmts,
                RETURN_STMT=return_stmt,
                NAMESPACE_CLOSE="",
            )
            method_impls.append(method_impl)
            is_first = False

    # Add namespace close to last method
    if namespace_close and method_impls:
        method_impls[-1] = method_impls[-1].rstrip() + namespace_close

    return "\n".join(method_impls)


def cpp_client_impl_gen(*extra_includes):
    extra_includes_expr = "\n".join(extra_includes) if extra_includes else ""

    client_impls = []
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        client_impl = _print_client_impl(struct_type)
        if client_impl:
            client_impls.append(client_impl)

    client_impls_expr = "\n".join(client_impls)

    result = CPP_CLIENT_IMPL_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        CLIENT_IMPLS_EXPR=client_impls_expr,
    )

    cb.set_result(result)
    return cb.get_result()


def cpp_client_interface_gen(*extra_includes):
    use_rpc = False
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        if len(struct_type._rpc) > 0:
            use_rpc = True
            break

    rpc_include = "#include <rbc_ipc/command_list.h>" if use_rpc else ""
    extra_includes_expr = "\n".join(extra_includes) if extra_includes else ""

    client_classes = []
    for struct_name in tr._registed_struct_types:
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        client_code = _print_client_code(struct_type)
        if client_code:
            client_classes.append(client_code)

    client_classes_expr = "\n".join(client_classes)

    result = CPP_CLIENT_INTERFACE_TEMPLATE.substitute(
        RPC_INCLUDE=rpc_include,
        EXTRA_INCLUDES=extra_includes_expr,
        CLIENT_CLASSES_EXPR=client_classes_expr,
    )

    cb.set_result(result)
    return cb.get_result()


def cpp_interface_gen(*extra_includes):
    global cb
    INDENT = DEFAULT_INDENT

    extra_includes_expr = "\n".join(extra_includes)

    def get_enum_expr(enum_name: str):
        enum_type: tr.enum = tr._registed_enum_types[enum_name]

        if enum_type._cpp_external:
            return ""

        enum_kvpairs = ",\n".join(
            [
                CPP_ENUM_KVPAIR_TEMPLATE.substitute(
                    INDENT=INDENT,
                    KEY=kv[0],
                    VALUE_EXPR=f"= {kv[1]}" if kv[1] is not None else "",
                )
                for kv in enum_type._params
            ]
        )
        enum_expr = CPP_ENUM_TEMPLATE.substitute(
            INDENT=INDENT,
            NAMESPACE_NAME=enum_type.namespace_name(),
            ENUM_NAME=enum_type.class_name(),
            ENUM_KVPAIRS=enum_kvpairs,
        )
        return enum_expr

    enums_expr = "\n".join(
        [get_enum_expr(enum_name) for enum_name in tr._registed_enum_types]
    )

    def get_struct_expr(struct_name):
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        if struct_type._cpp_external:
            return ""

        def get_member_expr(mem_name):
            mem = struct_type._members[mem_name]
            initer = struct_type._cpp_initer.get(mem_name)
            init_expr = initer if initer else ""
            var_type_name = get_arg_type(mem)
            member_expr = CPP_STRUCT_MEMBER_EXPR_TEMPLATE.substitute(
                INDENT=INDENT,
                VAR_TYPE_NAME=var_type_name,
                MEMBER_NAME=mem_name,
                INIT_EXPR=init_expr,
            )
            return member_expr

        members_expr = "\n".join(
            [get_member_expr(mem_name) for mem_name in struct_type._members]
        )

        has_serde = (
            len(struct_type._members) > 0 and len(struct_type._serde_members) > 0
        )

        ser_decl = (
            CPP_STRUCT_SER_DECL_TEMPLATE.substitute(FUNC_API=struct_type._suffix)
            if has_serde
            else ""
        )

        deser_decl = (
            CPP_STRUCT_DESER_DECL_TEMPLATE.substitute(FUNC_API=struct_type._suffix)
            if has_serde
            else ""
        )

        def get_method_expr(func: tr._function_t, method_name: str):
            ret_type = (
                get_arg_type(func._ret_type, False, False) if func.ret_type else "void"
            )
            args_expr = _print_arg_vars_decl(func._args, True, False, True)
            return CPP_STRUCT_METHOD_DECL_TEMPLATE.substitute(
                INDENT=INDENT,
                RET_TYPE=ret_type,
                FUNC_NAME=method_name,
                ARGS_EXPR=args_expr,
            )

        methods_list = []

        # print methods
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]
                methods_list.append(get_method_expr(func, mem_name))

        methods_decl = "\n".join(methods_list)

        name = struct_type.full_name()
        m = hashlib.md5(name.encode("ascii"))
        is_first = True
        digest = ""
        for i in m.digest():
            if not is_first:
                digest += ", "
            is_first = False
            digest += str(int(i))

        def get_rpc_expr(rpc):
            rpc_list = []
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
                        args += f"{get_arg_type(func._args[arg_name])} {arg_name}"
                    static = ""
                    if func._static:
                        static = "static "
                    rpc_list.append(
                        f"{static}{get_arg_type(func._ret_type)} {func_name}({args});"
                    )
            return "\n".join(rpc_list)

        rpc_expr = "" if len(struct_type._rpc) == 0 else get_rpc_expr(struct_type._rpc)

        struct_expr = CPP_STRUCT_TEMPLATE.substitute(
            NAMESPACE_NAME=struct_type.namespace_name(),
            STRUCT_NAME=struct_type.class_name(),
            INDENT=INDENT,
            FUNC_API=struct_type._suffix,
            MEMBERS_EXPR=members_expr,
            SER_DECL=ser_decl,
            DESER_DECL=deser_decl,
            RPC_FUNC_DECL=rpc_expr,
            USER_DEFINED_METHODS_DECL=methods_decl,
            MD5_DIGEST=digest,
        )

        return struct_expr

    structs_expr = "\n".join(
        [get_struct_expr(struct_name) for struct_name in tr._registed_struct_types]
    )

    cpp_interface_expr = CPP_INTERFACE_TEMPLATE.substitute(
        EXTRA_INCLUDE=extra_includes_expr,
        ENUMS_EXPR=enums_expr,
        STRUCTS_EXPR=structs_expr,
    )
    cb.add_result(cpp_interface_expr)

    return cb.get_result()


def pybind_codegen(module_name: str, *extra_includes):
    INDENT = DEFAULT_INDENT
    export_func_name = f"export_{module_name}"
    extra_includes_expr = "\n".join(extra_includes) if extra_includes else ""
    ptr_name = "ptr_484111b5e8ed4230b5ef5f5fdc33ca81"  # magic name

    def get_enum_binding(enum_name: str):
        enum_type: tr.enum = tr._registed_enum_types[enum_name]
        if enum_type._py_external:
            return ""

        enum_values = "\n".join(
            [
                PYBIND_ENUM_VALUE_TEMPLATE.substitute(
                    INDENT=INDENT,
                    VALUE_NAME=kv[0],
                    ENUM_NAME=enum_name,
                )
                for kv in enum_type._params
            ]
        )

        return PYBIND_ENUM_BINDING_TEMPLATE.substitute(
            INDENT=INDENT,
            ENUM_NAME=enum_name,
            CLASS_NAME=enum_type.class_name(),
            ENUM_VALUES=enum_values,
        )

    enum_bindings = [
        get_enum_binding(enum_name) for enum_name in tr._registed_enum_types
    ]
    enum_bindings_expr = "\n".join([e for e in enum_bindings if e])

    def get_struct_bindings(struct_name: str):
        struct_type: tr.struct = tr._registed_struct_types[struct_name]
        if struct_type._py_external:
            return ""

        result_parts = []

        # create function
        create_name = f"create__{struct_name}__"
        create_func = PYBIND_CREATE_FUNC_TEMPLATE.substitute(
            INDENT=INDENT,
            CREATE_NAME=create_name,
            STRUCT_NAME=struct_name,
        )
        result_parts.append(create_func)

        # dispose function
        dispose_name = f"dispose__{struct_name}__"
        dispose_func = PYBIND_DISPOSE_FUNC_TEMPLATE.substitute(
            INDENT=INDENT,
            DISPOSE_NAME=dispose_name,
            PTR_NAME=ptr_name,
            STRUCT_NAME=struct_name,
        )
        result_parts.append(dispose_func)

        # method functions
        for mem_name in struct_type._methods:
            mems_dict: dict = struct_type._methods[mem_name]
            for key in mems_dict:
                func: tr._function_t = mems_dict[key]

                ret_type = ""
                return_expr = ""
                return_close = ""
                if func._ret_type:
                    ret_type = f" -> {get_arg_type(func._ret_type, True, False)}"
                    return_expr = "return "
                    f = pybind_return_pass.get(func._ret_type)
                    if f:
                        return_expr += f(func._ret_type) + "("
                        return_close = ")"

                args_decl = _print_arg_vars_decl(func._args, False, True, True)
                args_call = _print_arg_vars(func._args, True, True)

                method_func = PYBIND_METHOD_FUNC_TEMPLATE.substitute(
                    INDENT=INDENT,
                    METHOD_NAME=f"{struct_name}__{mem_name}__",
                    PTR_NAME=ptr_name,
                    ARGS_DECL=args_decl,
                    RET_TYPE=ret_type,
                    RETURN_EXPR=return_expr,
                    STRUCT_NAME=struct_name,
                    METHOD_NAME_CALL=mem_name,
                    ARGS_CALL=args_call,
                    RETURN_CLOSE=return_close,
                )
                result_parts.append(method_func)

        return "\n".join(result_parts)

    struct_bindings = [
        get_struct_bindings(struct_name) for struct_name in tr._registed_struct_types
    ]
    struct_bindings_expr = "\n".join([s for s in struct_bindings if s])

    enum_initers_expr, struct_impls_expr = _cpp_impl_gen()
    impl_code = (
        f"{enum_initers_expr}\n\n{struct_impls_expr}"
        if enum_initers_expr or struct_impls_expr
        else ""
    )

    result = PYBIND_CODE_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        EXPORT_FUNC_NAME=export_func_name,
        ENUM_BINDINGS=enum_bindings_expr,
        STRUCT_BINDINGS=struct_bindings_expr,
        IMPL_CODE=impl_code,
    )

    cb.set_result(result)
    return cb.get_result()
