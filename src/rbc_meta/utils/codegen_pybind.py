from rbc_meta.utils.reflect import ReflectionRegistry
from rbc_meta.utils.codegen_util import to_include_expr, _write_string_to
from rbc_meta.utils.templates import (
    DEFAULT_INDENT,
    CPP_ENUM_INITER_TEMPLATE,
    CPP_STRUCT_REGIST_TEMPLATE,
    CPP_STRUCT_SER_IMPL_TEMPLATE,
    CPP_STRUCT_DESER_IMPL_TEMPLATE,
    PYBIND_CODE_TEMPLATE,
)
from rbc_meta.utils.pybind_codegen import (
    pybind_enum_binding,
    pybind_struct_bindings)
import hashlib
from pathlib import Path
from typing import List, TYPE_CHECKING

if TYPE_CHECKING:
    from rbc_meta.utils.codegenx import CodeModule

def gen_pybind_cpp_impl(mod: "CodeModule", dep_mods: List["CodeModule"] = []):
        reg = ReflectionRegistry()
        INDENT = DEFAULT_INDENT
        print("Dependencies: [")
        extra_headers = list(mod.header_files)
        for dep_mod in dep_mods:
            print("- " + dep_mod.name)
            extra_headers.extend(dep_mod.header_files)
        print("]")

        print(f"Collected {len(extra_headers)} Header Files")
        for header in extra_headers:
            print("- " + header)

        extra_include_expr = "\n".join([to_include_expr(x) for x in extra_headers])

        enum_bindings = []
        struct_bindings = []
        enum_initers_list = []
        struct_impls_list = []
        export_func_name = f"export_{mod.name}"

        for cls in mod.classes:
            info = reg.get_class_info(cls.__name__)
            namespace_name = info.cpp_namespace or ""
            enum_binding = pybind_enum_binding(info)
            if enum_binding:
                enum_bindings.append(enum_binding)

            struct_binding = pybind_struct_bindings(info, reg)
            if struct_binding:
                struct_bindings.append(struct_binding)

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

        enum_bindings_expr = "\n".join(enum_bindings)
        struct_bindings_expr = "\n".join(struct_bindings)

        file_expr = PYBIND_CODE_TEMPLATE.substitute(
            EXTRA_INCLUDES=extra_include_expr,
            EXPORT_FUNC_NAME=export_func_name,
            ENUM_BINDINGS=enum_bindings_expr,
            STRUCT_BINDINGS=struct_bindings_expr,
        )
        pybind_cpp_path = Path(mod.pybind_cpp_def_file).resolve()
        _write_string_to(file_expr, pybind_cpp_path)