import hashlib
from typing import Dict, List, Optional, Any, Type, Union, get_origin, get_args
import inspect

from rbc_meta.utils.reflect import (
    ReflectionRegistry,
    ClassInfo,
    MethodInfo,
    FieldInfo,
)
from rbc_meta.utils.templates import (
    DEFAULT_INDENT,
    CPP_ENUM_TEMPLATE,
    CPP_ENUM_KVPAIR_TEMPLATE,
    CPP_ENUM_INITER_TEMPLATE,
    CPP_STRUCT_TEMPLATE,
    CPP_STRUCT_MEMBER_EXPR_TEMPLATE,
    CPP_STRUCT_SER_DECL_TEMPLATE,
    CPP_STRUCT_DESER_DECL_TEMPLATE,
    CPP_STRUCT_METHOD_DECL_TEMPLATE,
    CPP_INTERFACE_TEMPLATE,
    CPP_IMPL_TEMPLATE,
    CPP_STRUCT_SER_IMPL_TEMPLATE,
    CPP_STRUCT_DESER_IMPL_TEMPLATE,
    CPP_STRUCT_RPC_METHOD_DECL_TEMPLATE,
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
    CPP_STRUCT_BUILTIN_METHODS_TEMPLATE,
    PY_MODULE_TEMPLATE,
    PY_INTERFACE_CLASS_TEMPLATE,
    PY_ENUM_EXPR_TEMPLATE,
    PY_ENUM_VALUE_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE_EXTERNAL,
    PY_DISPOSE_METHOD_TEMPLATE,
    PY_METHOD_TEMPLATE,
    PYBIND_CODE_TEMPLATE,
    PYBIND_METHOD_NAME_TEMPLATE,
    PYBIND_ENUM_BINDING_TEMPLATE,
    PYBIND_ENUM_VALUE_TEMPLATE,
    PYBIND_CREATE_FUNC_TEMPLATE,
    PYBIND_METHOD_FUNC_TEMPLATE,
)


_PYBIND_SPECUAL_ARG = {
    "luisa::span<std::byte>": "to_span_5d4636ab",
    "luisa::function<void()> const&": "to_cppfunc_5d4636ab",
    "py::memoryview": "to_memoryview_5d4636ab",
}

# Type name functions for special types


def _print_str(t, py_interface: bool = False, is_view: bool = False) -> str:
    if is_view:
        return "luisa::string_view"
    elif py_interface:
        return "luisa::string"
    else:
        return "luisa::string"


def _print_guid(t, py_interface: bool = False, is_view: bool = False) -> str:
    if py_interface:
        return "GuidData"
    elif is_view:
        return "vstd::Guid const&"
    else:
        return "vstd::Guid"


def _print_data_buffer(t, py_interface: bool = False, is_view: bool = False) -> str:
    if py_interface:
        if is_view:
            return "py::buffer const&"
        else:
            return "py::memoryview"
    else:
        return "luisa::span<std::byte>"


def _print_callback(t, py_interface: bool = False, is_view: bool = False) -> str:
    if py_interface:
        if is_view:
            return "py::function const&"
        else:
            raise ImportError("callback from c++ not supported.")
    else:
        return "luisa::function<void()> const&"


_TYPE_NAME_FUNCTIONS = {
    str: _print_str,
    # GUID and DataBuffer would need to be imported/defined
    # For now, we'll handle them in _get_cpp_type if needed
}

# Import GUID from builtin to check type
try:
    from rbc_meta.utils.builtin import GUID as BuiltinGUID
    from rbc_meta.utils.builtin import DataBuffer as BuiltinDataBuffer
    from rbc_meta.utils.builtin import Callback as BuiltinCallback

    _TYPE_NAME_FUNCTIONS[BuiltinGUID] = _print_guid
    _TYPE_NAME_FUNCTIONS[BuiltinDataBuffer] = _print_data_buffer
    _TYPE_NAME_FUNCTIONS[BuiltinCallback] = _print_callback

except ImportError:
    pass

# Python type names for type hints
_PY_NAMES = {
    int: "int",
    float: "float",
    str: "str",
    bool: "bool",
    BuiltinDataBuffer: "",
}


# 获取meta类型对应的cpp类型
# None -> void
# Specific String
# _cpp_type_name


def _get_cpp_type(
    type_hint: Type, py_interface: bool = False, is_view: bool = True
) -> str:
    """Map Python type to C++ type string."""
    if type_hint is None:
        return "void"

    # The Override Type Name Function
    f = _TYPE_NAME_FUNCTIONS.get(type_hint)
    if f:
        return f(type_hint, py_interface, is_view)

    # Handle Generic types FIRST (before checking _cpp_type_name)
    # This is important for nested generics like Vector[Vector[int]]
    if hasattr(type_hint, "__origin__"):
        origin = type_hint.__origin__
        args = getattr(type_hint, "__args__", ())

        # Check if is pointer

        # Check if origin is a custom container type (Vector, UnorderedMap, etc.)
        if (
            hasattr(origin, "_cpp_type_name")
            and hasattr(origin, "_is_container")
            and origin._is_container
        ):
            cpp_name = origin._cpp_type_name
            if len(args) == 1:
                inner_type = _get_cpp_type(args[0], py_interface, is_view)
                return f"{cpp_name}<{inner_type}>"
            elif len(args) == 2:
                key_type = _get_cpp_type(args[0], py_interface, is_view)
                value_type = _get_cpp_type(args[1], py_interface, is_view)
                return f"{cpp_name}<{key_type}, {value_type}>"

        # Handle standard Python generic types
        if isinstance(origin, list):
            assert len(args) == 1  # vector should have 1 arg
            return f"luisa::vector<{_get_cpp_type(args[0], py_interface, is_view)}>"
        elif isinstance(origin, dict):
            assert len(args) == 2  # dict should have key/value pair
            return f"luisa::unordered_map<{_get_cpp_type(args[0], py_interface, is_view)}, {_get_cpp_type(args[1], py_interface, is_view)}>"
        elif isinstance(origin, set):
            assert len(args) == 1
            return (
                f"luisa::unordered_set<{_get_cpp_type(args[0], py_interface, is_view)}>"
            )
        else:
            print(f"unsupported generic type: {origin}")

    # Handle basic types mapped in builtin.py or standard python types
    if hasattr(type_hint, "cpp_type_name"):
        return type_hint.cpp_type_name(py_interface, is_view)

    if hasattr(type_hint, "_cpp_type_name"):
        return type_hint._cpp_type_name

    if hasattr(type_hint, "__name__"):
        name = type_hint.__name__
        if name == "bool":
            return "bool"
        elif name == "int":
            return "int32_t"
        elif name == "float":
            return "float"
        elif name == "str":
            return "luisa::string"

    # Fallback to class name (assuming it's a registered type)
    if hasattr(type_hint, "__name__"):
        return type_hint.__name__

    return "void"


def _get_full_cpp_type(
    type_hint: Any,
    registry: ReflectionRegistry,
    py_interface: bool = False,
    is_view: bool = False,
) -> str:
    """Get full C++ type name with namespace if available."""
    # Try to find if it's a registered class to get namespace
    cpp_type = _get_cpp_type(type_hint, py_interface, is_view)

    # Check if the type itself is registered
    info = None
    if (
        hasattr(type_hint, "_pybind_type_")
        and type_hint._pybind_type_
        and (not hasattr(type_hint, "_is_enum_") or not type_hint._is_enum_)
    ):
        return "void*"
    if hasattr(type_hint, "__name__"):
        # Try to find by name in registry
        for key, cls_info in registry.get_all_classes().items():
            if cls_info.cls == type_hint:
                info = cls_info
                break

    if info and info.cpp_namespace:
        if info.cpp_namespace in cpp_type:  # Already has namespace?
            return cpp_type
        return f"{info.cpp_namespace}::{cpp_type}"
    # elif not info:
    #     return "void*"
    return cpp_type


def _get_py_type(type_hint: Any) -> Optional[str]:
    """Get Python type name for type hints."""
    f = _PY_NAMES.get(type_hint)
    if f is not None:
        return f
    if hasattr(type_hint, "_py_type_name"):
        if len(type_hint._py_type_name) > 0:
            return type_hint._py_type_name
        return None

    # For class types (not instances), return the class name
    if isinstance(type_hint, type):
        if hasattr(type_hint, "__name__"):
            name = type_hint.__name__
            # Basic types are already handled above
            if name not in ("bool", "int", "float", "str"):
                # Map uint and ulong to int for Python type hints
                if name in ("uint", "ulong"):
                    return "int"
                return name

    # For instances (like ExternalType instances), don't generate type hints
    # Check if it's a registered class
    return None


def _print_arg_vars_decl(
    parameters: Dict[str, inspect.Parameter],
    is_first: bool,
    py_interface: bool,
    is_view: bool,
    registry: ReflectionRegistry,
) -> str:
    """Print argument variable declarations."""
    r = ""
    for param_name, param in parameters.items():
        if not is_first:
            r += ", "
        is_first = False
        param_type = (
            param.annotation if param.annotation != inspect.Signature.empty else None
        )
        r += _get_full_cpp_type(param_type, registry, py_interface, is_view)
        r += " "
        r += param_name
    return r


def _print_py_args_decl(
    parameters: Dict[str, inspect.Parameter], is_first: bool, self_type: Type
) -> str:
    """Print Python argument declarations with type hints."""
    r = ""
    for param_name, param in parameters.items():
        if not is_first:
            r += ", "
        is_first = False
        param_type = (
            param.annotation if param.annotation != inspect.Signature.empty else None
        )
        type_str = None
        if not (
            hasattr(param_type, "__name__")
            and self_type.__name__ == param_type.__name__
        ):
            type_str = _get_py_type(param_type) if param_type else None
        r += param_name
        if type_str:
            r += ": " + type_str
    return r


def _print_py_args(
    parameters: Dict[str, inspect.Parameter],
    is_first: bool,
    is_cpp: bool,
    registry: ReflectionRegistry | None = None,
) -> str:
    """Print Python argument names for function calls."""
    r = ""
    for param_name, param in parameters.items():
        if not is_first:
            r += ", "
        is_first = False
        arg_open = ""
        arg_close = ""
        if is_cpp:
            type_name = _get_full_cpp_type(
                param.annotation
                if param.annotation != inspect.Signature.empty
                else None,
                registry,
            )
            arg_parse = _PYBIND_SPECUAL_ARG.get(type_name)
            if arg_parse:
                arg_open = arg_parse + "("
                arg_close = ")"
        else:
            param_type = (
                param.annotation
                if param.annotation != inspect.Signature.empty
                else None
            )
            if (
                param_type
                and hasattr(param_type, "_pybind_type_")
                and param_type._pybind_type_ and (
                    hasattr(param_type, "_is_enum_") and 
                    not param_type._is_enum_
                )
            ):
                arg_close = "._handle" + arg_close
        # type_str = _get_py_type(param_type) if param_type else None
        r += arg_open + param_name + arg_close
    return r


def _is_rpc_method(method: MethodInfo) -> bool:
    """Check if a method is marked as RPC."""
    # Check if method has is_rpc attribute (from MethodInfo dataclass)
    return method.is_rpc


def _get_rpc_methods(info: ClassInfo) -> Dict[str, List[MethodInfo]]:
    """Get RPC methods grouped by name."""
    rpc_methods = {}
    for method in info.methods:
        if _is_rpc_method(method):
            if method.name not in rpc_methods:
                rpc_methods[method.name] = []
            rpc_methods[method.name].append(method)
    return rpc_methods


def _print_rpc_serializer(struct_type: ClassInfo, registry: ReflectionRegistry) -> str:
    """Generate RPC serializer code for a struct."""
    INDENT = DEFAULT_INDENT
    rpc_methods = _get_rpc_methods(struct_type)
    if len(rpc_methods) == 0:
        return ""

    full_name = (
        f"{struct_type.cpp_namespace}::{struct_type.name}"
        if struct_type.cpp_namespace
        else struct_type.name
    )

    arg_structs = []
    func_names_list = []
    call_exprs_list = []
    arg_metas_list = []
    ret_metas_list = []
    is_statics_list = []

    for func_name, method_list in rpc_methods.items():
        for method in method_list:
            func_hasher_name = hashlib.md5(
                str(full_name + "->" + func_name + "|" + str(method.signature)).encode(
                    "ascii"
                )
            ).hexdigest()

            # Filter out 'self' parameter for RPC arg struct
            rpc_params = {k: v for k, v in method.parameters.items() if k != "self"}

            arg_struct_name = "void"
            if len(rpc_params) > 0:
                arg_struct_name = f"Arg{func_hasher_name}"

                arg_members = "\n".join(
                    [
                        CPP_RPC_ARG_MEMBER_TEMPLATE.substitute(
                            INDENT=INDENT,
                            ARG_TYPE=_get_full_cpp_type(
                                param.annotation
                                if param.annotation != inspect.Signature.empty
                                else None,
                                registry,
                            ),
                            ARG_NAME=param_name,
                        )
                        for param_name, param in rpc_params.items()
                    ]
                )

                ser_stmts = "\n".join(
                    [
                        CPP_RPC_SER_STMT_TEMPLATE.substitute(
                            INDENT=INDENT,
                            ARG_NAME=param_name,
                        )
                        for param_name in rpc_params
                    ]
                )

                deser_stmts = "\n".join(
                    [
                        CPP_RPC_DESER_STMT_TEMPLATE.substitute(
                            INDENT=INDENT,
                            ARG_NAME=param_name,
                        )
                        for param_name in rpc_params
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
            # Check if method is static (from MethodInfo)
            is_static = method.is_static
            is_statics_list.append("true" if is_static else "false")

            self_param = "void *self, " if not is_static else ""
            args_cast = ""
            if len(rpc_params) > 0:
                args_cast = f"{INDENT}{INDENT}auto args_ptr = static_cast<{arg_struct_name} *>(args);\n"

            is_ret_void = not (method.return_type and method.return_type != type(None))
            ret_type_name = _get_full_cpp_type(method.return_type, registry)

            ret_construct = ""
            if not is_ret_void:
                ret_construct = f"{INDENT}{INDENT}std::construct_at(static_cast<{ret_type_name} *>(ret_value),\n"

            func_call_prefix = (
                f"{full_name}::{func_name}("
                if is_static
                else f"static_cast<{full_name} *>(self)->{func_name}("
            )
            args_call = (
                ", ".join([f"args_ptr->{param_name}" for param_name in rpc_params])
                if len(rpc_params) > 0
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

    hash_name = hashlib.md5(full_name.encode("ascii")).hexdigest()

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


def cpp_interface_gen(module_filter: List[str] = [], *extra_includes) -> str:
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT

    extra_includes_expr = "\n".join(extra_includes)

    structs_expr_list = []
    enums_expr_list = []

    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()

    for key, info in all_classes:
        # Filter Builtin
        if info.module == "builtin":
            continue

        # Filter out module not selected
        if len(module_filter) > 0 and info.module not in module_filter:
            continue

        namespace_name = info.cpp_namespace
        class_name = info.name

        if info.is_enum:
            # Generate enum
            enum_kvpairs = ",\n".join(
                [
                    CPP_ENUM_KVPAIR_TEMPLATE.substitute(
                        INDENT=INDENT,
                        KEY=field.name,
                        VALUE_EXPR=f"= {field.default}"
                        if field.default is not None
                        else "",
                    )
                    for field in info.fields
                ]
            )
            enum_expr = CPP_ENUM_TEMPLATE.substitute(
                INDENT=INDENT,
                NAMESPACE_NAME=namespace_name or "",
                ENUM_NAME=class_name,
                ENUM_KVPAIRS=enum_kvpairs,
            )
            enums_expr_list.append(enum_expr)
            continue

        # Members
        members_list = []
        for field in info.fields:
            # Determine C++ type
            var_type_name = "void"

            # Use generic info if available
            if field.generic_info:
                # if is pure pointer
                if field.generic_info.is_pointer:
                    assert len(field.generic_info.args) == 1
                    inner_type = _get_full_cpp_type(
                        field.generic_info.args[0], registry
                    )
                    var_type_name = f"{inner_type}*"
                # if is custom generic type
                elif field.generic_info.cpp_name:
                    # Handle different container types
                    if len(field.generic_info.args) == 1:
                        # Single parameter containers (Vector, etc.)
                        inner_type = _get_full_cpp_type(
                            field.generic_info.args[0], registry
                        )
                        var_type_name = f"{field.generic_info.cpp_name}<{inner_type}>"
                    elif len(field.generic_info.args) == 2:
                        # Two parameter containers (UnorderedMap, etc.)
                        key_type = _get_full_cpp_type(
                            field.generic_info.args[0], registry
                        )
                        value_type = _get_full_cpp_type(
                            field.generic_info.args[1], registry
                        )
                        var_type_name = (
                            f"{field.generic_info.cpp_name}<{key_type}, {value_type}>"
                        )
                    else:
                        # Fallback to using the type directly
                        var_type_name = _get_full_cpp_type(field.type, registry)
                else:
                    var_type_name = _get_full_cpp_type(field.type, registry)
            else:
                var_type_name = _get_full_cpp_type(field.type, registry)

            # 使用 C++ 初始化表达式（如果提供），否则使用默认值
            init_expr = ""
            if field.cpp_init_expr:
                init_expr = field.cpp_init_expr
            elif field.default is not None:
                # 尝试将 Python 默认值转换为 C++ 初始化表达式
                if isinstance(field.default, bool):
                    init_expr = "true" if field.default else "false"
                elif isinstance(field.default, (int, float)):
                    init_expr = str(field.default)
                elif isinstance(field.default, str):
                    init_expr = f'"{field.default}"'

            member_expr = CPP_STRUCT_MEMBER_EXPR_TEMPLATE.substitute(
                INDENT=INDENT,
                VAR_TYPE_NAME=var_type_name,
                MEMBER_NAME=field.name,
                INIT_EXPR=init_expr,
            )
            members_list.append(member_expr)

        members_expr = "\n".join(members_list)

        # Serde declarations
        func_api = info.cpp_prefix
        has_serde = info.serde and len(info.fields) > 0

        ser_decl = CPP_STRUCT_SER_DECL_TEMPLATE.substitute() if has_serde else ""
        deser_decl = CPP_STRUCT_DESER_DECL_TEMPLATE.substitute() if has_serde else ""

        # Methods
        methods_list = []
        for method in info.methods:
            if _is_rpc_method(method):
                continue  # RPC methods are handled separately
            ret_type = (
                _get_full_cpp_type(method.return_type, registry, False, False)
                if method.return_type
                else "void"
            )
            # Filter out 'self' parameter for C++ method declarations
            method_params = {k: v for k, v in method.parameters.items() if k != "self"}
            args_expr = _print_arg_vars_decl(
                method_params, False, False, True, registry
            )
            method_expr = CPP_STRUCT_METHOD_DECL_TEMPLATE.substitute(
                INDENT=INDENT,
                RET_TYPE=ret_type,
                FUNC_NAME=method.name,
                ARGS_EXPR=args_expr,
            )
            methods_list.append(method_expr)

        methods_decl = "\n".join(methods_list)

        # RPC methods
        rpc_methods = _get_rpc_methods(info)
        rpc_list = []
        for func_name, method_list in rpc_methods.items():
            for method in method_list:
                is_static = method.is_static
                # Filter out 'self' parameter for RPC declarations
                rpc_params = {k: v for k, v in method.parameters.items() if k != "self"}
                args = _print_arg_vars_decl(rpc_params, True, False, False, registry)
                static = "static " if is_static else ""
                ret_type = (
                    _get_full_cpp_type(method.return_type, registry)
                    if method.return_type
                    else "void"
                )

                # rpc_list.append(f"{static}{ret_type} {func_name}({args});")
                rpc_list.append(
                    CPP_STRUCT_RPC_METHOD_DECL_TEMPLATE.substitute(
                        INDENT=INDENT,
                        STATIC_EXPR=static,
                        RET_TYPE=ret_type,
                        FUNC_NAME=func_name,
                        ARGS_EXPR=args,
                    )
                )

        rpc_expr = "\n".join(rpc_list)

        # MD5 Digest
        full_name = f"{namespace_name}::{class_name}" if namespace_name else class_name
        m = hashlib.md5(full_name.encode("ascii"))
        digest = ", ".join(str(b) for b in m.digest())

        built_in_methods_decl = (
            ""
            if not info.pybind or not info.create_instance
            else CPP_STRUCT_BUILTIN_METHODS_TEMPLATE.substitute(
                INDENT=INDENT, STRUCT_NAME=class_name
            )
        )
        struct_base_expr = ": ::rbc::RBCStruct"
        # TODO: we don't want to inherit in cpp
        # if len(info.base_classes) == 1:
        #     base_class = info.base_classes[0]
        #     assert base_class is not None
        #     base_expr = _get_cpp_type(base_class.cls)
        #     struct_base_expr = f": public {base_expr}"
        #     # only on rttr type, valid
        # elif len(info.base_classes) > 1:
        #     # should not happen
        #     print(f"{class_name} has more than 1 base classes")

        # print(f"{class_name}: {info.base_classes}")

        struct_expr = CPP_STRUCT_TEMPLATE.substitute(
            NAMESPACE_NAME=namespace_name or "",
            FUNC_API=func_api,
            STRUCT_NAME=class_name,
            STRUCT_BASE_EXPR=struct_base_expr,
            INDENT=INDENT,
            BUILT_IN_METHODS_EXPR=built_in_methods_decl,
            MEMBERS_EXPR=members_expr,
            SER_DECL=ser_decl,
            DESER_DECL=deser_decl,
            RPC_METHODS_DECL=rpc_expr,
            USER_DEFINED_METHODS_DECL=methods_decl,
            MD5_DIGEST=digest,
        )
        structs_expr_list.append(struct_expr)

    structs_expr = "\n".join(structs_expr_list)
    enums_expr = "\n".join(enums_expr_list)

    return CPP_INTERFACE_TEMPLATE.substitute(
        EXTRA_INCLUDE=extra_includes_expr,
        ENUMS_EXPR=enums_expr,
        STRUCTS_EXPR=structs_expr,
    )


def cpp_impl_gen(module_filter: List[str] = [], *extra_includes) -> str:
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT

    extra_includes_expr = "\n".join(extra_includes)

    struct_impls_list = []
    enum_initers_list = []

    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()

    for key, info in all_classes:
        # print(f"Registering {key}")
        if len(module_filter) > 0 and info.module not in module_filter:
            continue

        namespace_name = info.cpp_namespace or ""
        class_name = info.name

        if info.is_enum:
            # Generate enum initer
            full_name = (
                f"{namespace_name}::{class_name}" if namespace_name else class_name
            )
            m = hashlib.md5(full_name.encode("ascii"))
            digest = m.hexdigest()

            enum_names = ", ".join([f'"{field.name}"' for field in info.fields])
            # For enum values, use the default value or index
            enum_values = ", ".join(
                [
                    f"(uint64_t){field.default}"
                    if field.default is not None
                    else f"(uint64_t){i}"
                    for i, field in enumerate(info.fields)
                ]
            )

            initer = f'"{full_name}", std::initializer_list<char const*>{{{enum_names}}}, std::initializer_list<uint64_t>{{{enum_values}}}'

            enum_initer = CPP_ENUM_INITER_TEMPLATE.substitute(
                DIGEST=digest,
                INITER=initer,
            )
            enum_initers_list.append(enum_initer)
            continue

        # Serde Impl (only if serde is enabled)
        if info.serde and len(info.fields) > 0:
            store_stmts_list = []
            load_stmts_list = []

            for field in info.fields:
                # 检查字段级别的 serde 设置
                # field.serde == None 表示使用类级别的 serde 设置
                # field.serde == True 表示序列化
                # field.serde == False 表示不序列化
                should_serde = info.serde
                # print(f"{field.name}: {field.serde}")
                if field.serde is not None:
                    should_serde = field.serde

                if should_serde:
                    store_stmts_list.append(
                        f'{INDENT}{INDENT}obj._store(this->{field.name}, "{field.name}");'
                    )
                    load_stmts_list.append(
                        f'{INDENT}{INDENT}obj._load(this->{field.name}, "{field.name}");'
                    )

            store_stmts = "\n".join(store_stmts_list)
            load_stmts = "\n".join(load_stmts_list)

            ser_impl = CPP_STRUCT_SER_IMPL_TEMPLATE.substitute(
                NAMESPACE_NAME=namespace_name,
                CLASS_NAME=class_name,
                STORE_STMTS=store_stmts,
            )

            deser_impl = CPP_STRUCT_DESER_IMPL_TEMPLATE.substitute(
                CLASS_NAME=class_name,
                LOAD_STMTS=load_stmts,
                NAMESPACE_NAME=namespace_name,
            )

            struct_impls_list.append(ser_impl)
            struct_impls_list.append(deser_impl)

        # RPC serializer
        rpc_serializer = _print_rpc_serializer(info, registry)
        if rpc_serializer:
            struct_impls_list.append(rpc_serializer)

    struct_impls_expr = "\n".join(struct_impls_list)
    enum_initers_expr = "\n".join(enum_initers_list)

    return CPP_IMPL_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        ENUM_INITERS_EXPR=enum_initers_expr,
        STRUCT_IMPLS_EXPR=struct_impls_expr,
    )


# Constants for client code generation
JSON_SER_NAME = "b839f6ccb4b74"
SELF_NAME = "d6922fb0e4bd44549"


def py_interface_gen(module_name: str, module_filter: List[str] = []) -> str:
    """Generate Python interface code."""
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT

    # def get_enum_expr(key: str, info: ClassInfo):
    #     print(f"Generating Enum for {info.name}")
    #     if not info.is_enum:
    #         return ""

    #     enum_name = info.name
    #     enum_values = "\n".join(
    #         [
    #             PY_ENUM_VALUE_TEMPLATE.substitute(
    #                 INDENT=INDENT,
    #                 VALUE_NAME=info.fields[i].name,
    #                 VALUE_EXPR=f"= {info.fields[i].default}"
    #                 if info.fields[i].default is not None
    #                 else "",
    #             )
    #             for i in range(len(info.fields))
    #         ]
    #     )

    #     return PY_ENUM_EXPR_TEMPLATE.substitute(
    #         ENUM_NAME=enum_name, ENUM_VALUES=enum_values
    #     )
    type_to_cls_info = {}

    def get_class_expr(key: str, info: ClassInfo):
        if info.is_enum:
            return "", []

        struct_name = info.name  # Use class name as struct name for C++ binding
        if not info.pybind or not info.create_instance:
            init_method = PY_INIT_METHOD_TEMPLATE_EXTERNAL.substitute(INDENT=INDENT)
            dispose_method = ""
        else:
            init_method = PY_INIT_METHOD_TEMPLATE.substitute(
                INDENT=INDENT,
                STRUCT_NAME=struct_name,
            )

            dispose_method = PY_DISPOSE_METHOD_TEMPLATE.substitute(
                INDENT=INDENT
            )

        pybind_methods_list = []
        if info.create_instance:
            pybind_methods_list.append(f"create__{struct_name}__")

        def get_method_expr(method: MethodInfo, type: Type):
            # Filter out 'self' parameter for Python method declarations
            method_params = {k: v for k, v in method.parameters.items() if k != "self"}
            args_decl = _print_py_args_decl(method_params, False, type)
            args_call = _print_py_args(method_params, False, False)
            return_expr = "return " if method.return_type else ""
            return_end = ""
            if (
                method.return_type
                and hasattr(method.return_type, "_pybind_type_")
                and method.return_type._pybind_type_
                and (
                    not hasattr(method.return_type, "_is_enum_")
                    or not method.return_type._is_enum_
                )
            ):
                return_expr += _get_py_type(method.return_type) + "("
                return_end = ")"

            pybind_method_name = PYBIND_METHOD_NAME_TEMPLATE.substitute(
                STRUCT_NAME=struct_name,
                METHOD_NAME=method.name,
            )
            pybind_methods_list.append(pybind_method_name)
            return PY_METHOD_TEMPLATE.substitute(
                INDENT=INDENT,
                METHOD_NAME=method.name,
                ARGS_DECL=args_decl,
                RETURN_EXPR=return_expr,
                PYBIND_METHOD_NAME=pybind_method_name,
                ARGS_CALL=args_call,
                RETURN_END=return_end,
            )

        def get_inherit_method_expr(method: MethodInfo, struct_name: str, type: Type):
            method_params = {k: v for k, v in method.parameters.items() if k != "self"}
            args_decl = _print_py_args_decl(method_params, False, type)
            args_call = _print_py_args(method_params, False, False)
            return_expr = "return " if method.return_type else ""
            return_end = ""
            if (
                method.return_type
                and hasattr(method.return_type, "_pybind_type_")
                and method.return_type._pybind_type_
                and not method.return_type._is_enum_
            ):
                return_expr += _get_py_type(method.return_type) + "("
                return_end = ")"
            pybind_method_name = PYBIND_METHOD_NAME_TEMPLATE.substitute(
                STRUCT_NAME=struct_name,
                METHOD_NAME=method.name,
            )
            return PY_METHOD_TEMPLATE.substitute(
                INDENT=INDENT,
                METHOD_NAME=method.name,
                ARGS_DECL=args_decl,
                RETURN_EXPR=return_expr,
                PYBIND_METHOD_NAME=pybind_method_name,
                ARGS_CALL=args_call,
                RETURN_END=return_end,
            )

        methods_list = []
        for method in info.methods:
            if _is_rpc_method(method):
                continue  # Skip RPC methods in Python interface
            methods_list.append(get_method_expr(method, info.cls))

        def print_inherit(info: ClassInfo):
            inherit_cls_infos = []
            if info.inherit:
                if type(info.inherit) != list:
                    inherit_cls_info = type_to_cls_info.get(info.inherit)
                    if inherit_cls_info:
                        inherit_cls_infos.append(inherit_cls_info)
                else:
                    for i in info.inherit:
                        inherit_cls_info = type_to_cls_info.get(i)
                        if inherit_cls_info:
                            inherit_cls_infos.append(inherit_cls_info)
            for inherit_cls_info in inherit_cls_infos:
                for method in inherit_cls_info.methods:
                    methods_list.append(
                        get_inherit_method_expr(method, inherit_cls_info.name, info.cls)
                    )
                print_inherit(inherit_cls_info)

        print_inherit(info)
        methods_expr = "".join(methods_list)

        return PY_INTERFACE_CLASS_TEMPLATE.substitute(
            CLASS_NAME=info.name,
            INIT_METHOD=init_method,
            DISPOSE_METHOD=dispose_method,
            METHODS_EXPR=methods_expr,
        ), pybind_methods_list

    classes_expr_list = []
    enum_exprs = []

    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()
    for key, info in all_classes:
        type_to_cls_info[info.cls] = info
    for key, info in all_classes:
        if len(module_filter) > 0 and info.module not in module_filter:
            continue

        if not info.pybind:  # filter out classes marked pybind
            continue

        # enum_expr = get_enum_expr(key, info)
        # if enum_expr:
        #     enum_exprs.append(enum_expr)

        class_expr, pybind_methods_list = get_class_expr(key, info)
        if class_expr:
            classes_expr_list.append(class_expr)

    classes_expr = "\n".join(classes_expr_list)
    enum_exprs = "\n".join(enum_exprs)
    module_expr = f"from rbc_ext._C.{module_name} import *"

    result = PY_MODULE_TEMPLATE.substitute(
        MODULE_EXPR=module_expr,
        ENUM_EXPRS=enum_exprs,
        CLASS_EXPRS=classes_expr,
    )

    return result


def _print_client_code(struct_type: ClassInfo, registry: ReflectionRegistry) -> str:
    """Generate C++ client interface code."""
    INDENT = DEFAULT_INDENT
    rpc_methods = _get_rpc_methods(struct_type)
    if len(rpc_methods) == 0:
        return ""

    def get_method_decl(method: MethodInfo):
        # Filter out 'self' parameter for client method declarations
        rpc_params = {k: v for k, v in method.parameters.items() if k != "self"}
        args_decl = ", ".join(
            [
                f"{_get_full_cpp_type(param.annotation if param.annotation != inspect.Signature.empty else None, registry)} {param_name}"
                for param_name, param in rpc_params.items()
            ]
        )
        if args_decl:
            args_decl = ", " + args_decl

        ret_type = "void"
        if method.return_type and method.return_type is not type(None):
            ret_type = (
                f"rbc::RPCFuture<{_get_full_cpp_type(method.return_type, registry)}>"
            )

        is_static = method.is_static
        self_param = ", void*" if not is_static else ""

        return CPP_CLIENT_METHOD_DECL_TEMPLATE.substitute(
            INDENT=INDENT,
            RET_TYPE=ret_type,
            METHOD_NAME=method.name,
            SELF_PARAM=self_param,
            ARGS_DECL=args_decl,
        )

    method_decls = []
    for func_name, method_list in rpc_methods.items():
        for method in method_list:
            method_decls.append(get_method_decl(method))

    methods_decl = "\n".join(method_decls)

    namespace_name = struct_type.cpp_namespace or ""

    return CPP_CLIENT_CLASS_TEMPLATE.substitute(
        NAMESPACE_NAME=namespace_name,
        CLASS_NAME=struct_type.name,
        METHOD_DECLS=methods_decl,
    )


def _print_client_impl(struct_type: ClassInfo, registry: ReflectionRegistry) -> str:
    """Generate C++ client implementation code."""
    INDENT = DEFAULT_INDENT
    rpc_methods = _get_rpc_methods(struct_type)
    if len(rpc_methods) == 0:
        return ""

    class_name = struct_type.name
    namespace_name = struct_type.cpp_namespace
    namespace_expr = (
        "" if not struct_type.cpp_namespace else struct_type.cpp_namespace + "::"
    )

    full_name = (
        f"{struct_type.cpp_namespace}::{class_name}"
        if struct_type.cpp_namespace
        else class_name
    )
    method_impls = []

    for func_name, method_list in rpc_methods.items():
        for method in method_list:
            func_hasher_name = hashlib.md5(
                str(full_name + "->" + func_name + "|" + str(method.signature)).encode(
                    "ascii"
                )
            ).hexdigest()

            # Filter out 'self' parameter for client method declarations
            rpc_params = {k: v for k, v in method.parameters.items() if k != "self"}
            args_decl = ", ".join(
                [
                    f"{_get_full_cpp_type(param.annotation if param.annotation != inspect.Signature.empty else None, registry)} {param_name}"
                    for param_name, param in rpc_params.items()
                ]
            )
            if args_decl:
                args_decl = ", " + args_decl

            ret_type_name = _get_full_cpp_type(method.return_type, registry)
            ret_type = "void"
            if method.return_type and method.return_type is not type(None):
                ret_type = f"rbc::RPCFuture<{ret_type_name}>"

            is_static = method.is_static
            self_param = f", void* {SELF_NAME}" if not is_static else ""
            self_arg = f", {SELF_NAME}" if not is_static else ""

            add_args_stmts = "\n".join(
                [
                    CPP_CLIENT_ADD_ARG_STMT_TEMPLATE.substitute(
                        INDENT=INDENT,
                        JSON_SER_NAME=JSON_SER_NAME,
                        ARG_NAME=param_name,
                    )
                    for param_name in rpc_params
                ]
            )

            return_stmt = ""
            if method.return_type and method.return_type is not type(None):
                return_stmt = CPP_CLIENT_RETURN_STMT_TEMPLATE.substitute(
                    INDENT=INDENT,
                    JSON_SER_NAME=JSON_SER_NAME,
                    RET_TYPE=ret_type_name,
                )

            method_impl = CPP_CLIENT_METHOD_IMPL_TEMPLATE.substitute(
                INDENT=INDENT,
                RET_TYPE=ret_type,
                NAMESPACE_EXPR=namespace_expr,
                CLASS_NAME=class_name,
                METHOD_NAME=func_name,
                JSON_SER_NAME=JSON_SER_NAME,
                SELF_PARAM=self_param,
                ARGS_DECL=args_decl,
                FUNC_HASH=func_hasher_name,
                SELF_ARG=self_arg,
                ADD_ARGS_STMTS=add_args_stmts,
                RETURN_STMT=return_stmt,
            )
            method_impls.append(method_impl)

    return "\n".join(method_impls)


def cpp_client_interface_gen(module_filter: List[str] = [], *extra_includes) -> str:
    """Generate C++ client interface header."""
    registry = ReflectionRegistry()
    extra_includes_expr = "\n".join(extra_includes)

    # Check if any struct has RPC methods
    use_rpc = False
    for key, info in registry.get_all_classes().items():
        if len(module_filter) > 0 and info.module not in module_filter:
            continue
        if len(_get_rpc_methods(info)) > 0:
            use_rpc = True
            break

    rpc_include = "#include <rbc_ipc/command_list.h>" if use_rpc else ""

    client_classes = []
    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()
    for key, info in all_classes:
        if module_filter and info.module not in module_filter:
            continue
        client_code = _print_client_code(info, registry)
        if client_code:
            client_classes.append(client_code)

    client_classes_expr = "\n".join(client_classes)

    return CPP_CLIENT_INTERFACE_TEMPLATE.substitute(
        RPC_INCLUDE=rpc_include,
        EXTRA_INCLUDES=extra_includes_expr,
        CLIENT_CLASSES_EXPR=client_classes_expr,
    )


def cpp_client_impl_gen(module_filter: List[str] = [], *extra_includes) -> str:
    """Generate C++ client implementation."""
    registry = ReflectionRegistry()
    extra_includes_expr = "\n".join(extra_includes)

    client_impls = []
    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()

    for key, info in all_classes:
        if len(module_filter) > 0 and info.module not in module_filter:
            continue
        client_impl = _print_client_impl(info, registry)
        if client_impl:
            client_impls.append(client_impl)

    client_impls_expr = "\n".join(client_impls)

    return CPP_CLIENT_IMPL_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        CLIENT_IMPLS_EXPR=client_impls_expr,
    )


def pybind_codegen(
    module_name: str, module_filter: List[str] = [], *extra_includes
) -> str:
    """Generate Pybind11 binding code."""
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT
    export_func_name = f"export_{module_name}"
    extra_includes_expr = "\n".join(extra_includes) if extra_includes else ""
    ptr_name = "ptr_484111b5e"  # magic name

    def get_enum_binding(key: str, info: ClassInfo):
        if not info.is_enum:
            return ""
        enum_name = info.name
        namespace_name = info.cpp_namespace or ""
        enum_values = "\n".join(
            [
                PYBIND_ENUM_VALUE_TEMPLATE.substitute(
                    INDENT=INDENT,
                    VALUE_NAME=field.name,
                    ENUM_NAME=enum_name,  # Use full key for enum name
                )
                for field in info.fields
            ]
        )

        return PYBIND_ENUM_BINDING_TEMPLATE.substitute(
            INDENT=INDENT,
            NAMESPACE_NAME=namespace_name,
            ENUM_NAME=enum_name,
            CLASS_NAME=info.name,
            ENUM_VALUES=enum_values,
        )

    enum_bindings = []
    struct_bindings = []

    def get_struct_bindings(key: str, info: ClassInfo):
        if info.is_enum:
            return ""
        if not info.pybind:
            return ""

        result_parts = []
        class_name = info.name  # Use class name instead of full key
        # Use full namespace-qualified name for C++ code
        namespace_name = info.cpp_namespace or ""
        struct_name = (
            f"{namespace_name}::{class_name}" if namespace_name else class_name
        )

        # create function
        if not info.pybind or not info.create_instance:
            create_func = ""
        else:
            create_name = f"create__{class_name}__"
            create_func = PYBIND_CREATE_FUNC_TEMPLATE.substitute(
                INDENT=INDENT,
                CREATE_NAME=create_name,
                STRUCT_NAME=struct_name,
            )
        result_parts.append(create_func)

        ## No custom dispose, all use RC
        # dispose function
        # dispose_name = f"dispose__{class_name}__"
        # if not info.pybind or not info.create_instance:
        #     dispose_func = ""
        # else:
        #     dispose_func = PYBIND_DISPOSE_FUNC_TEMPLATE.substitute(
        #         INDENT=INDENT,
        #         DISPOSE_NAME=dispose_name,
        #         PTR_NAME=ptr_name,
        #         STRUCT_NAME=struct_name,
        #     )
        # result_parts.append(dispose_func)

        # method functions
        for method in info.methods:
            if _is_rpc_method(method):
                continue  # Skip RPC methods

            ret_type = ""
            return_expr = ""
            return_close = ""
            if method.return_type:
                # Get the return type for pybind (py_interface=True)
                pybind_ret_type = _get_full_cpp_type(
                    method.return_type, registry, True, False
                )
                # Get the actual C++ method return type (is_view=True for interface methods)

                ret_type = f" -> {pybind_ret_type}"
                return_expr = "return "

                # If C++ method returns string_view but pybind expects string, add conversion
                # if (
                #     cpp_ret_type == "luisa::string_view"
                #     and pybind_ret_type == "luisa::string"
                # ):
                #     # return_expr += "luisa::string("
                #     # return_close = ")"
                #     pass
                # if C++ method returns DataBuffer
                arg_parse = _PYBIND_SPECUAL_ARG.get(pybind_ret_type)
                if arg_parse:
                    return_expr += arg_parse + "("
                    return_close = ")"
                else:
                    return_close = ""

            # Filter out 'self' parameter for pybind method bindings
            method_params = {k: v for k, v in method.parameters.items() if k != "self"}
            args_decl = _print_arg_vars_decl(method_params, False, True, True, registry)
            args_call = _print_py_args(method_params, False, True, registry)

            method_func = PYBIND_METHOD_FUNC_TEMPLATE.substitute(
                INDENT=INDENT,
                METHOD_NAME=f"{class_name}__{method.name}__",
                PTR_NAME=ptr_name,
                ARGS_DECL=args_decl,
                RET_TYPE=ret_type,
                RETURN_EXPR=return_expr,
                STRUCT_NAME=struct_name,
                METHOD_NAME_CALL=method.name,
                ARGS_CALL=args_call,
                RETURN_CLOSE=return_close,
            )
            result_parts.append(method_func)

        return "\n".join(result_parts)

    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()
    for key, info in all_classes:
        if len(module_filter) > 0 and info.module not in module_filter:
            continue

        enum_binding = get_enum_binding(key, info)
        if enum_binding:
            enum_bindings.append(enum_binding)

        struct_binding = get_struct_bindings(key, info)
        if struct_binding:
            struct_bindings.append(struct_binding)

    enum_bindings_expr = "\n".join(enum_bindings)
    struct_bindings_expr = "\n".join(struct_bindings)

    # Generate impl code (reuse cpp_impl_gen logic)
    enum_initers_list = []
    struct_impls_list = []

    for key, info in all_classes:
        if module_filter and info.module not in module_filter:
            continue

        namespace_name = info.cpp_namespace or ""
        class_name = info.name

        if info.is_enum:
            full_name = (
                f"{namespace_name}::{class_name}" if namespace_name else class_name
            )
            m = hashlib.md5(full_name.encode("ascii"))
            digest = m.hexdigest()

            enum_names = ", ".join([f'"{field.name}"' for field in info.fields])
            enum_values = ", ".join(
                [
                    f"(uint64_t){field.default}"
                    if field.default is not None
                    else f"(uint64_t){i}"
                    for i, field in enumerate(info.fields)
                ]
            )

            initer = f'"{full_name}", std::initializer_list<char const*>{{{enum_names}}}, std::initializer_list<uint64_t>{{{enum_values}}}'

            enum_initer = CPP_ENUM_INITER_TEMPLATE.substitute(
                DIGEST=digest,
                INITER=initer,
            )
            enum_initers_list.append(enum_initer)

        else:
            if info.serde and len(info.fields) > 0:
                store_stmts_list = []
                load_stmts_list = []

                for field in info.fields:
                    # 检查字段级别的 serde 设置
                    # field.serde == None 表示使用类级别的 serde 设置
                    # field.serde == True 表示序列化
                    # field.serde == False 表示不序列化
                    should_serde = info.serde
                    if field.serde is not None:
                        should_serde = field.serde

                    if should_serde:
                        store_stmts_list.append(
                            f"{INDENT}{INDENT}obj._store(this->{field.name});"
                        )
                        load_stmts_list.append(
                            f"{INDENT}{INDENT}obj._load(this->{field.name});"
                        )

                store_stmts = "\n".join(store_stmts_list)
                load_stmts = "\n".join(load_stmts_list)

                namespace_expr = (
                    f"namespace {namespace_name} {{" if namespace_name else ""
                )

                ser_impl = CPP_STRUCT_SER_IMPL_TEMPLATE.substitute(
                    NAMESPACE_NAME=namespace_expr,
                    CLASS_NAME=class_name,
                    STORE_STMTS=store_stmts,
                )

                deser_impl = CPP_STRUCT_DESER_IMPL_TEMPLATE.substitute(
                    CLASS_NAME=class_name,
                    LOAD_STMTS=load_stmts,
                    NAMESPACE_NAME=namespace_expr,
                )

                struct_impls_list.append(ser_impl)
                struct_impls_list.append(deser_impl)

            rpc_serializer = _print_rpc_serializer(info, registry)
            if rpc_serializer:
                struct_impls_list.append(rpc_serializer)

    return PYBIND_CODE_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        EXPORT_FUNC_NAME=export_func_name,
        ENUM_BINDINGS=enum_bindings_expr,
        STRUCT_BINDINGS=struct_bindings_expr,
    )
