from rbc_meta.utils.reflect import (
    ReflectionRegistry,
    ClassInfo,
    MethodInfo,
    FieldInfo,
)
from rbc_meta.utils.templates import (
    DEFAULT_INDENT,
    PYBIND_ENUM_BINDING_TEMPLATE,
    PYBIND_ENUM_VALUE_TEMPLATE,
    PYBIND_CREATE_FUNC_TEMPLATE,
    PYBIND_METHOD_FUNC_TEMPLATE)

from rbc_meta.utils.codegen_util import _get_full_cpp_type, _print_arg_vars_decl
import inspect
from typing import Dict, Optional, Any, Type

_PYBIND_SPECUAL_ARG = {
    "luisa::span<std::byte>": "to_span_5d4636ab",
    "py::memoryview": "to_memoryview_5d4636ab",
    "GuidData": "GuidData"
}

# Python type names for type hints
_PY_NAMES = {
    int: "int",
    float: "float",
    str: "str",
    bool: "bool",
}


def _get_py_type(type_hint: Any) -> Optional[str]:
    """Get Python type name for type hints."""
    f = _PY_NAMES.get(type_hint)
    if f is not None:
        return f

    # Handle Generic types FIRST (before checking _cpp_type_name)
    # This is important for nested generics like Vector[Vector[int]]
    if hasattr(type_hint, "__origin__"):
        origin = type_hint.__origin__
        args = getattr(type_hint, "__args__", ())
        return None  # discard containers
        if (
            hasattr(origin, "_py_type_name")
            and hasattr(origin, "_is_container")
            and origin._is_container
        ):
            cpp_name = origin._py_type_name
            if len(args) == 1:
                inner_type = _get_py_type(args[0])
                return f"{cpp_name}[{inner_type}]"
            elif len(args) == 2:
                key_type = _get_py_type(args[0])
                value_type = _get_py_type(args[1])
                return f"{cpp_name}<{key_type}, {value_type}>"

        # Handle standard Python generic types
        if isinstance(origin, list):
            assert len(args) == 1  # vector should have 1 arg
            return f"List[{_get_py_type(args[0])}]"
        elif isinstance(origin, dict):
            assert len(args) == 2  # dict should have key/value pair
            return f"Dict[{_get_py_type(args[0])}, {_get_py_type(args[1])}]"
        else:
            print(f"unsupported generic type: {origin}")

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

    return None


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
            arg_parse = None
            if hasattr(param.annotation, '_cpp_arg_call'):
                arg_parse = param.annotation._cpp_arg_call
            if arg_parse is None:
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
                and param_type._pybind_type_
                and (hasattr(param_type, "_is_enum_") and not param_type._is_enum_)
            ):
                arg_close = "._handle" + arg_close
        # type_str = _get_py_type(param_type) if param_type else None
        r += arg_open + param_name + arg_close
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




def pybind_struct_bindings(info: ClassInfo, registry: ReflectionRegistry, INDENT: str=DEFAULT_INDENT):
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
        ptr_name = "ptr_484111b5e"  # magic name

        # method functions
        for method in info.methods:
            if method.is_rpc:
                continue  # Skip RPC methods
            if method.is_inherit_func:
                continue

            # ret_type = ""
            return_expr = ""
            return_close = ""
            # NO need return type
            
            if method.return_type:
                # Get the return type for pybind (py_interface=True)
                pybind_ret_type = _get_full_cpp_type(
                    method.return_type, registry, True, False
                )
                # ret_type = f" -> {pybind_ret_type}"
                return_expr = "return "
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
                # RET_TYPE=ret_type,
                RETURN_EXPR=return_expr,
                STRUCT_NAME=struct_name,
                METHOD_NAME_CALL=method.name,
                ARGS_CALL=args_call,
                RETURN_CLOSE=return_close,
            )
            result_parts.append(method_func)

        return "\n".join(result_parts)

def pybind_enum_binding(info: ClassInfo, INDENT: str = DEFAULT_INDENT):
    if not info.is_enum:
        return ""
    enum_name = info.name
    namespace_name = info.cpp_namespace or ""
    enum_values = "\n".join(
        [
            PYBIND_ENUM_VALUE_TEMPLATE.substitute(
                NAMESPACE_NAME=namespace_name,
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


