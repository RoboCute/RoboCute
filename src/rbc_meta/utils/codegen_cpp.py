"""
C++ 代码生成实现
"""

import hashlib
from pathlib import Path
from typing import TYPE_CHECKING, List
from rbc_meta.utils.reflect import ReflectionRegistry
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
    CPP_STRUCT_REGIST_TEMPLATE,
    CPP_STRUCT_SER_IMPL_TEMPLATE,
    CPP_STRUCT_DESER_IMPL_TEMPLATE,
    CPP_STRUCT_BUILTIN_METHODS_TEMPLATE,
)
from rbc_meta.utils.codegen_util import (
    _write_string_to,
    _get_full_cpp_type,
    _print_arg_vars_decl,
    to_include_expr,
    _get_interface_header_from_path,
)

if TYPE_CHECKING:
    from rbc_meta.utils.codegenx import CodeModule
    from rbc_meta.utils.reflect import ClassInfo


def gen_cpp_impl(mod: "CodeModule"):
    """生成 C++ 实现文件"""
    reg = ReflectionRegistry()
    INDENT = DEFAULT_INDENT

    struct_impls_list = []
    enum_initers_list = []

    # 从头文件路径推断接口头文件包含路径
    interface_header = _get_interface_header_from_path(mod.cpp_interface_header)
    extra_includes_expr = to_include_expr(interface_header)

    for info in mod.classes:
        info = reg.get_class_info(info.__name__)
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
            continue

        if info.serde and len(info.fields) > 0:
            store_stmts_list = []
            load_stmts_list = []
            for field in info.fields:
                should_serde = info.serde
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

    file_expr = CPP_IMPL_TEMPLATE.substitute(
        EXTRA_INCLUDES=extra_includes_expr,
        ENUM_INITERS_EXPR=enum_initers_expr,
        STRUCT_IMPLS_EXPR=struct_impls_expr,
    )

    cpp_path = Path(mod.cpp_impl_file).resolve()
    _write_string_to(file_expr, cpp_path)


def gen_cpp_interface_header(mod: "CodeModule", dep_mods: List["CodeModule"] = []):
    """生成 C++ 接口头文件"""
    reg = ReflectionRegistry()

    print("Dependencies: [")
    extra_headers = list(mod.header_files)
    for dep_mod in dep_mods:
        print("- " + dep_mod.name)
        extra_headers.extend(dep_mod.header_files)
    print("]")
    print(f"Collected {len(extra_headers)} Header Files")
    for header in extra_headers:
        print("- " + header)

    structs_expr = []
    enums_expr = []
    extra_include_expr = "\n".join([to_include_expr(x) for x in extra_headers])

    for info in mod.classes:
        info = reg.get_class_info(info.__name__)
        if info.is_enum:
            enums_expr.append(enum_gen(info))
        else:
            structs_expr.append(cpp_struct_def_gen(info))

    enums_expr = "\n".join(enums_expr)
    structs_expr = "\n".join(structs_expr)

    header_path = Path(mod.cpp_interface_header).resolve()
    file_expr = CPP_INTERFACE_TEMPLATE.substitute(
        EXTRA_INCLUDE=extra_include_expr,
        ENUMS_EXPR=enums_expr,
        STRUCTS_EXPR=structs_expr,
    )
    _write_string_to(file_expr, header_path)


def cpp_struct_def_gen(info: "ClassInfo") -> str:
    """生成 C++ 结构体定义"""
    INDENT = DEFAULT_INDENT
    registry = ReflectionRegistry()
    namespace_name = info.cpp_namespace
    class_name = info.name
    members_list = []
    for field in info.fields:
        var_type_name = "void"

        if field.generic_info:
            if field.generic_info.is_pointer:
                assert len(field.generic_info.args) == 1
                inner_type = _get_full_cpp_type(
                    field.generic_info.args[0], registry
                )
                var_type_name = f"{inner_type}*"
            elif field.generic_info.cpp_name:
                if len(field.generic_info.args) == 1:
                    inner_type = _get_full_cpp_type(
                        field.generic_info.args[0], registry
                    )
                    var_type_name = f"{field.generic_info.cpp_name}<{inner_type}>"
                elif len(field.generic_info.args) == 2:
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
                    var_type_name = _get_full_cpp_type(field.type, registry)
            else:
                var_type_name = _get_full_cpp_type(field.type, registry)
        else:
            var_type_name = _get_full_cpp_type(field.type, registry)

        init_expr = ""
        if field.cpp_init_expr:
            init_expr = field.cpp_init_expr
        elif field.default is not None:
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

    func_api = info.cpp_prefix
    has_serde = info.serde and len(info.fields) > 0

    ser_decl = CPP_STRUCT_SER_DECL_TEMPLATE.substitute() if has_serde else ""
    deser_decl = CPP_STRUCT_DESER_DECL_TEMPLATE.substitute() if has_serde else ""

    methods_list = []
    for method in info.methods:
        if method.is_inherit_func:
            continue

        ret_type = (
            _get_full_cpp_type(method.return_type, registry, False, False)
            if method.return_type
            else "void"
        )

        method_params = {k: v for k, v in method.parameters.items() if k != "self"}
        args_expr = _print_arg_vars_decl(
            method_params,
            False,
            False,
            True,
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
    return struct_expr


def enum_gen(info: "ClassInfo"):
    """生成 C++ 枚举定义"""
    INDENT = DEFAULT_INDENT
    namespace_name = info.cpp_namespace
    class_name = info.name
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

    return enum_expr
