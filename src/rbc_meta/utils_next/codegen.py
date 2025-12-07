import hashlib
from typing import Dict, List, Optional, Any, Type
import inspect

from rbc_meta.utils_next.reflect import (
    ReflectionRegistry,
    ClassInfo,
    MethodInfo,
    FieldInfo,
)
from rbc_meta.utils_next.templates import (
    DEFAULT_INDENT,
    CPP_STRUCT_TEMPLATE,
    CPP_STRUCT_MEMBER_EXPR_TEMPLATE,
    CPP_STRUCT_SER_DECL_TEMPLATE,
    CPP_STRUCT_DESER_DECL_TEMPLATE,
    CPP_INTERFACE_TEMPLATE,
    CPP_IMPL_TEMPLATE,
    CPP_STRUCT_SER_IMPL_TEMPLATE,
    CPP_STRUCT_DESER_IMPL_TEMPLATE,
)


def _get_cpp_type(type_hint: Any) -> str:
    """Map Python type to C++ type string."""
    if type_hint is None:
        return "void"

    # Handle basic types mapped in builtin.py or standard python types

    # Handle basic types mapped in builtin.py or standard python types
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
            return "luisa::string"  # Align with old codegen

    # Handle Generic types (Vector[T])
    # utils_next.reflect stores generic info in FieldInfo, but here we only have type_hint if called directly.
    # But usually we process FieldInfo which has generic_info.
    # For now, let's try to handle basic generic if passed as type
    if hasattr(type_hint, "__origin__"):
        origin = type_hint.__origin__
        args = type_hint.__args__
        if hasattr(origin, "_cpp_type_name"):
            inner = _get_cpp_type(args[0])
            return f"{origin._cpp_type_name}<{inner}>"

    # Fallback to class name (assuming it's a registered type)
    if hasattr(type_hint, "__name__"):
        # Check if it's a registered class to get full name with namespace?
        # For now return name. Namespace handling might be needed.
        return type_hint.__name__

    return "void"


def _get_full_cpp_type(type_hint: Any, registry: ReflectionRegistry) -> str:
    # Try to find if it's a registered class to get namespace
    cpp_type = _get_cpp_type(type_hint)

    # Check if the type itself is registered
    info = None
    if hasattr(type_hint, "__name__"):
        # Try to find by name in registry?
        # This is slow, but we don't have module info easily here unless we look it up.
        # But wait, we can check registry by class object?
        # Registry keys are string.
        for key, cls_info in registry.get_all_classes().items():
            if cls_info.cls == type_hint:
                info = cls_info
                break

    if info and info.cpp_namespace:
        if info.cpp_namespace in cpp_type:  # Already has namespace?
            return cpp_type
        return f"{info.cpp_namespace}::{cpp_type}"

    return cpp_type


def cpp_interface_gen(module_filter: List[str] = None, *extra_includes) -> str:
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT

    extra_includes_expr = "\n".join(extra_includes)

    structs_expr_list = []

    # Sort classes to ensure deterministic output (by name)
    all_classes = sorted(registry.get_all_classes().items(), key=lambda x: x[0])

    for key, info in all_classes:
        # Filter by module
        if module_filter and info.module not in module_filter:
            continue
        print(f"Generating Interface for {key} with {module_filter}")
        # It's a struct/class
        namespace_name = info.cpp_namespace
        struct_name = info.name

        if info.is_enum:
            # Skip enums for now as per requirement (PixelStorage is external)
            # If we need to generate enums, we need CPP_ENUM_TEMPLATE logic
            continue

        # Members
        members_list = []
        for field in info.fields:
            # Determine C++ type
            var_type_name = "void"

            # Use generic info if available
            if field.generic_info:
                if field.generic_info.cpp_name:
                    inner_type = _get_full_cpp_type(
                        field.generic_info.args[0], registry
                    )
                    var_type_name = f"{field.generic_info.cpp_name}<{inner_type}>"
                else:
                    var_type_name = _get_full_cpp_type(field.type, registry)
            else:
                var_type_name = _get_full_cpp_type(field.type, registry)

            init_expr = ""

            member_expr = CPP_STRUCT_MEMBER_EXPR_TEMPLATE.substitute(
                INDENT=INDENT,
                VAR_TYPE_NAME=var_type_name,
                MEMBER_NAME=field.name,
                INIT_EXPR=init_expr,
            )
            members_list.append(member_expr)

        members_expr = "\n".join(members_list)

        # Serde declarations
        func_api = "RBC_RUNTIME_API"

        ser_decl = CPP_STRUCT_SER_DECL_TEMPLATE.substitute(FUNC_API=func_api)
        deser_decl = CPP_STRUCT_DESER_DECL_TEMPLATE.substitute(FUNC_API=func_api)

        # Methods
        methods_decl = ""
        rpc_expr = ""

        # MD5 Digest
        full_name = (
            f"{namespace_name}::{struct_name}" if namespace_name else struct_name
        )
        m = hashlib.md5(full_name.encode("ascii"))
        digest = ", ".join(str(b) for b in m.digest())

        struct_expr = CPP_STRUCT_TEMPLATE.substitute(
            NAMESPACE_NAME=namespace_name,
            STRUCT_NAME=struct_name,
            INDENT=INDENT,
            FUNC_API=func_api,
            MEMBERS_EXPR=members_expr,
            SER_DECL=ser_decl,
            DESER_DECL=deser_decl,
            RPC_FUNC_DECL=rpc_expr,
            USER_DEFINED_METHODS_DECL=methods_decl,
            MD5_DIGEST=digest,
        )
        structs_expr_list.append(struct_expr)

    structs_expr = "\n".join(structs_expr_list)
    enums_expr = ""  # No enums generated

    return CPP_INTERFACE_TEMPLATE.substitute(
        EXTRA_INCLUDE=extra_includes_expr,
        ENUMS_EXPR=enums_expr,
        STRUCTS_EXPR=structs_expr,
    )


def cpp_impl_gen(module_filter: List[str] = None, *extra_includes) -> str:
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT

    extra_includes_expr = "\n".join(extra_includes)

    struct_impls_list = []

    all_classes = sorted(registry.get_all_classes().items(), key=lambda x: x[0])

    for key, info in all_classes:
        print(f"Registering {key}")
        if module_filter and info.module not in module_filter:
            continue

        if info.is_enum:
            continue

        namespace_name = info.cpp_namespace
        struct_name = info.name

        # Serde Impl
        store_stmts_list = []
        load_stmts_list = []

        for field in info.fields:
            store_stmts_list.append(f"{INDENT}{INDENT}obj._store(this->{field.name});")
            load_stmts_list.append(f"{INDENT}{INDENT}obj._load(this->{field.name});")

        store_stmts = "\n".join(store_stmts_list)
        load_stmts = "\n".join(load_stmts_list)

        ser_impl = CPP_STRUCT_SER_IMPL_TEMPLATE.substitute(
            NAMESPACE_NAME=namespace_name,
            CLASS_NAME=struct_name,
            STORE_STMTS=store_stmts,
        )

        deser_impl = CPP_STRUCT_DESER_IMPL_TEMPLATE.substitute(
            CLASS_NAME=struct_name,
            LOAD_STMTS=load_stmts,
            NAMESPACE_NAME=namespace_name,
        )

        struct_impls_list.append(ser_impl)
        struct_impls_list.append(deser_impl)

    struct_impls_expr = "\n".join(struct_impls_list)
    enum_initers_expr = ""

    return CPP_IMPL_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        ENUM_INITERS_EXPR=enum_initers_expr,
        STRUCT_IMPLS_EXPR=struct_impls_expr,
    )
