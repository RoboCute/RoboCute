from pathlib import Path
import os
import re
import hashlib

import hashlib
from typing import Dict, List, Optional, Any, Type, Union, get_origin, get_args
import inspect

from rbc_meta.utils.reflect import (
    ReflectionRegistry,
)

from rbc_meta.utils.builtin import (
    Pointer,
    Const,
    Ref,
    DataBuffer,
    GUID,
)  # special case

# Write Result String To
def _write_string_to(s: str, path: Path):
    data = s.encode("utf-8")
    new_md5 = hashlib.md5(data).hexdigest()
    print(f"Writing to {path}")
    if os.path.exists(path):
        with open(path, "rb") as f:
            old_data = f.read()
            old_md5 = hashlib.md5(old_data).hexdigest()

        if new_md5 == old_md5:
            return False

    Path(path).parent.mkdir(parents=True, exist_ok=True)

    with open(path, "wb") as f:
        f.write(data)
    return True


def codegen_to(out_path: Path):
    def _(callback, *args):
        s = callback(*args)
        _write_string_to(s, out_path)

    return _


def re_translate(text: str, replacements: dict):
    pattern = re.compile(
        "|".join(re.escape(k) for k in sorted(replacements, key=len, reverse=True))
    )
    updated_text = pattern.sub(lambda match: replacements[match.group(0)], text)
    return updated_text


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


def _get_cpp_type(
    type_hint: Type,
    py_interface: bool = False,
    is_view: bool = True,
    registry: ReflectionRegistry = None,
) -> str:
    """Map Python type to C++ type string."""
    if type_hint is None:
        return "void"
    # first check if info

    if isinstance(type_hint, str):
        # str type hint means later eval
        for key, cls_info in registry.get_all_classes().items():
            if cls_info.cls.__name__ == type_hint:
                return _get_cpp_type(cls_info.cls, py_interface, is_view, registry)

    # The Override Type Name Function
    f = _TYPE_NAME_FUNCTIONS.get(type_hint)
    if f:
        return f(type_hint, py_interface, is_view)

    # Handle Generic types FIRST (before checking _cpp_type_name)
    # This is important for nested generics like Vector[Vector[int]]
    if hasattr(type_hint, "__origin__"):
        origin = type_hint.__origin__
        args = getattr(type_hint, "__args__", ())
        # Check if origin is a custom container type (Vector, UnorderedMap, etc.)

        if origin is Pointer:
            cpp_type = _get_cpp_type(args[0], py_interface, is_view, registry)
            return f"{cpp_type}*"

        if origin is Const:
            cpp_type = _get_cpp_type(args[0], py_interface, is_view, registry)
            return f"{cpp_type} const"

        if origin is Ref:
            cpp_type = _get_cpp_type(args[0], py_interface, is_view, registry)
            return f"{cpp_type}&"

        if (
            hasattr(origin, "_cpp_type_name")
            and hasattr(origin, "_is_container")
            and origin._is_container
        ):
            if origin._cpp_type_name is not None and callable(origin._cpp_type_name):
                cpp_name = origin._cpp_type_name(py_interface, is_view)
            else:
                cpp_name = origin._cpp_type_name
                
            if hasattr(origin, "_pybind_cpp_name") and not is_view:
                cpp_name = origin._pybind_cpp_name

            if len(args) == 1:
                inner_type = _get_cpp_type(args[0], py_interface, is_view, registry)
                return f"{cpp_name}<{inner_type}>"
            elif len(args) == 2:
                key_type = _get_cpp_type(args[0], py_interface, is_view, registry)
                value_type = _get_cpp_type(args[1], py_interface, is_view, registry)
                return f"{cpp_name}<{key_type}, {value_type}>"

        # Handle standard Python generic types
        if isinstance(origin, list):
            assert len(args) == 1  # vector should have 1 arg
            return f"luisa::vector<{_get_cpp_type(args[0], py_interface, is_view, registry)}>"
        elif isinstance(origin, dict):
            assert len(args) == 2  # dict should have key/value pair
            return f"luisa::unordered_map<{_get_cpp_type(args[0], py_interface, is_view, registry)}, {_get_cpp_type(args[1], py_interface, is_view, registry)}>"
        elif isinstance(origin, set):
            assert len(args) == 1
            return f"luisa::unordered_set<{_get_cpp_type(args[0], py_interface, is_view, registry)}>"
        else:
            print(f"unsupported generic type: {origin}")

    # in most cases it will cover the requirement
    info = registry.get_class_info(type_hint.__name__)

    if hasattr(type_hint, "_cpp_type_name"):
        if info is not None and info.is_enum:
            # if enum, directly return
            if type_hint._cpp_type_name is not None and callable(type_hint._cpp_type_name):
                return type_hint._cpp_type_name(py_interface, is_view)
            else:
                return type_hint._cpp_type_name
        elif (
            hasattr(type_hint, "_pybind_type_") and type_hint._pybind_type_
            # and py_interface
        ):
            return "void*"
        else:
            if type_hint._cpp_type_name is not None and callable(type_hint._cpp_type_name):
                return type_hint._cpp_type_name(py_interface, is_view)
            else:
                return type_hint._cpp_type_name

    if type_hint is bool:
        return "bool"
    elif type_hint is int:
        return "int32_t"
    elif type_hint is float:
        return "float"

    return "void"

def _get_full_cpp_type(
    type_hint: Type,
    registry: ReflectionRegistry,
    py_interface: bool = False,
    is_view: bool = False,
) -> str:
    """Get full C++ type name with namespace if available."""
    # Check if the built-in cpp types
    cpp_type = _get_cpp_type(type_hint, py_interface, is_view, registry)
    return cpp_type

_TYPE_NAME_FUNCTIONS = {
    str: _print_str,
    DataBuffer: _print_data_buffer,
    GUID: _print_guid,
}


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


def to_include_expr(x):
    return f"#include <{x}>"
