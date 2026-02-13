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
    CPP_STRUCT_REGIST_TEMPLATE,
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
    PY_METHOD_DISPOSE_TEMPLATE,
    PYBIND_CODE_TEMPLATE,
    PYBIND_METHOD_NAME_TEMPLATE,
    PYBIND_ENUM_BINDING_TEMPLATE,
    PYBIND_ENUM_VALUE_TEMPLATE,
    PYBIND_CREATE_FUNC_TEMPLATE,
    PYBIND_METHOD_FUNC_TEMPLATE,
)

# Type name functions for special types
from rbc_meta.utils.codegen_util import (
    _get_full_cpp_type,
     _print_arg_vars_decl
)
from rbc_meta.utils.builtin import (
    Pointer,
    Const,
    Ref,
    DataBuffer,
    GUID,
)  # special case

from rbc_meta.utils.pybind_codegen import pybind_enum_binding, pybind_struct_bindings, _print_py_args, _get_py_type, _print_py_args_decl

def cpp_interface_gen(
    module_filter: List[str] = [], *extra_includes
) -> str:
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
            if method.is_inherit_func:
                continue  # cpp donot impl prev func

            ret_type = (
                _get_full_cpp_type(method.return_type, registry)
                if method.return_type
                else "void"
            )
            # Filter out 'self' parameter for C++ method declarations
            method_params = {k: v for k, v in method.parameters.items() if k != "self"}

            args_expr = _print_arg_vars_decl(
                method_params,
                False,  # not first, first method is void* _this
                False,  # pybind
                True,  # is_view
                registry,
            )
            method_expr = CPP_STRUCT_METHOD_DECL_TEMPLATE.substitute(
                INDENT=INDENT,
                RET_TYPE=ret_type,
                FUNC_NAME=method.name,
                ARGS_EXPR=args_expr,
            )
            methods_list.append(method_expr)

        methods_decl = "\n".join(methods_list)
        rpc_expr = ""

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
        # C-style static function implementation, no C++ inheritance
        struct_base_expr = ": ::rbc::RBCStruct"
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
            regist_impl = CPP_STRUCT_REGIST_TEMPLATE.substitute(
                NAMESPACE_NAME=namespace_name,
                CLASS_NAME=class_name,
            )

            struct_impls_list.append(ser_impl)
            struct_impls_list.append(deser_impl)
            struct_impls_list.append(regist_impl)


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


def py_interface_gen(module_name: str, module_filter: List[str] = [], extra_import: str = None) -> str:
    """Generate Python interface code."""
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT
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

            dispose_method = PY_DISPOSE_METHOD_TEMPLATE.substitute(INDENT=INDENT)

        pybind_methods_list = []
        if info.create_instance:
            pybind_methods_list.append(f"create__{struct_name}__")

        def get_method_expr(method: MethodInfo, type: Type):
            # print(method)
            # Filter out 'self' parameter for Python method declarations
            method_params = {k: v for k, v in method.parameters.items() if k != "self"}

            args_decl = _print_py_args_decl(method_params, False, type)
            args_call = _print_py_args(method_params, False, False)

            return_expr = "return " if method.return_type else ""
            return_end = ""
            if (method.return_type):
                if (hasattr(method.return_type, '_ctor_begin') or
                    (hasattr(method.return_type, "_pybind_type_")
                    and method.return_type._pybind_type_
                    and (
                        not hasattr(method.return_type, "_is_enum_")
                        or not method.return_type._is_enum_
                    ))
                ):
                    return_expr += _get_py_type(method.return_type)
                    if hasattr(method.return_type, '_ctor_begin') and method.return_type._ctor_begin:
                        return_expr += '.'
                        return_expr += method.return_type._ctor_begin
                    else:
                        return_expr += '('
                    if hasattr(method.return_type, '_ctor_end') and method.return_type._ctor_end:
                        return_end = method.return_type._ctor_end
                    else:
                        return_end += ")"

            pybind_method_name = PYBIND_METHOD_NAME_TEMPLATE.substitute(
                STRUCT_NAME=struct_name,
                METHOD_NAME=method.name,
            )

            pybind_methods_list.append(pybind_method_name)
            if method.name == 'dispose':
                return PY_METHOD_DISPOSE_TEMPLATE.substitute(
                    INDENT=INDENT,
                    METHOD_NAME=method.name,
                    ARGS_DECL=args_decl,
                    RETURN_EXPR=return_expr,
                    PYBIND_METHOD_NAME=pybind_method_name,
                    ARGS_CALL=args_call,
                    RETURN_END=return_end,
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
            if method.is_inherit_func:
                continue  # skip inherit methods in python interface
            methods_list.append(get_method_expr(method, info.cls))

        methods_expr = "".join(methods_list)
        inherit_expr = ""
        # print(f"Class {info.name} has {len(info.base_classes)} base classes")
        if len(info.base_classes) == 1:
            base_class = info.base_classes[0]
            assert base_class is not None
            base_expr = _get_py_type(base_class.cls)
            inherit_expr = f"({base_expr})"
            # only on rttr type, valid
        elif len(info.base_classes) > 1:
            # should not happen
            print(f"{info.name} has more than 1 base classes")
        return PY_INTERFACE_CLASS_TEMPLATE.substitute(
            CLASS_NAME=info.name,
            INHERIT_EXPR=inherit_expr,
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
    if extra_import is not None:
        module_expr += '\n'
        module_expr += extra_import
    result = PY_MODULE_TEMPLATE.substitute(
        MODULE_EXPR=module_expr,
        ENUM_EXPRS=enum_exprs,
        CLASS_EXPRS=classes_expr,
    )

    return result

def pybind_codegen(
    module_name: str, module_filter: List[str] = [], *extra_includes
) -> str:
    """Generate Pybind11 binding code."""
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT
    export_func_name = f"export_{module_name}"
    extra_includes_expr = "\n".join(extra_includes) if extra_includes else ""

    enum_bindings = []
    struct_bindings = []


    # Use original order from registry to preserve module-defined order
    all_classes = registry.get_all_classes().items()
    for key, info in all_classes:
        if len(module_filter) > 0 and info.module not in module_filter:
            continue

        enum_binding = pybind_enum_binding(info)
        if enum_binding:
            enum_bindings.append(enum_binding)

        struct_binding = pybind_struct_bindings(info, registry)

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
                
                regist_impl = CPP_STRUCT_REGIST_TEMPLATE.substitute(
                    NAMESPACE_NAME=namespace_expr,
                    CLASS_NAME=class_name,
                )

                struct_impls_list.append(ser_impl)
                struct_impls_list.append(deser_impl)
                struct_impls_list.append(regist_impl)

    return PYBIND_CODE_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        EXPORT_FUNC_NAME=export_func_name,
        ENUM_BINDINGS=enum_bindings_expr,
        STRUCT_BINDINGS=struct_bindings_expr,
    )
