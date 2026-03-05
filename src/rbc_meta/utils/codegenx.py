"""
代码生成框架 - 简洁版 API

核心设计：路径即意图
- 指定 cpp_interface_header → 生成 C++ 接口头文件
- 指定 cpp_impl_file → 生成 C++ 实现文件
- 指定 pybind_py_file → 生成 Python 绑定
- 指定 pybind_cpp_def_file → 生成 pybind C++ 定义
"""

from typing import Dict, List, Optional, Any, Type, get_type_hints, get_origin, get_args
from pathlib import Path
from rbc_meta.utils.reflect import ReflectionRegistry
from rbc_meta.utils.reflect import (
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
    CPP_STRUCT_REGIST_TEMPLATE,
    CPP_STRUCT_SER_IMPL_TEMPLATE,
    CPP_STRUCT_DESER_IMPL_TEMPLATE,
    CPP_STRUCT_BUILTIN_METHODS_TEMPLATE,
    PY_MODULE_TEMPLATE,
    PY_INTERFACE_CLASS_TEMPLATE,
    PY_METHOD_DISPOSE_TEMPLATE,
    PY_ENUM_EXPR_TEMPLATE,
    PY_ENUM_VALUE_TEMPLATE,
    PY_MODULE_IMPORT_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE_EXTERNAL,
    PY_DISPOSE_METHOD_TEMPLATE,
    PY_BOOL_METHOD_TEMPLATE,
    PY_METHOD_TEMPLATE,
    PYBIND_CODE_TEMPLATE,
    PYBIND_METHOD_NAME_TEMPLATE,
    PYBIND_ENUM_BINDING_TEMPLATE,
    PYBIND_ENUM_VALUE_TEMPLATE,
    PYBIND_CREATE_FUNC_TEMPLATE,
    PYBIND_METHOD_FUNC_TEMPLATE,
)
from rbc_meta.utils.codegen_util import _write_string_to, _get_full_cpp_type, _print_arg_vars_decl
import hashlib
from rbc_meta.utils.pybind_codegen import (
    pybind_enum_binding,
    pybind_struct_bindings,
    _print_py_args,
    _get_py_type,
    _print_py_args_decl,
)


def to_include_expr(x):
    return f"#include <{x}>"


class CodegenRegistry:
    """代码生成注册表（单例）"""
    _instance: Optional["CodegenRegistry"] = None
    _modules: Dict[str, "CodeModule"] = {}

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def register(self, cls: Type) -> Type:
        cls_inst = cls()
        self._modules[cls.__name__] = cls_inst
        return cls

    def generate(self):
        print(f"Generating {len(self._modules)} modules")
        
        for name, mod in self._modules.items():
            print("=========================")
            print(mod.name)
            
            # 路径即意图：指定路径即启用功能
            if mod.cpp_interface_header:
                self.gen_cpp_interface_header(mod)
                if mod.cpp_impl_file:
                    self.gen_cpp_impl(mod)

            if mod.pybind_py_file:
                self.gen_pybind_py(mod)

            if mod.pybind_cpp_def_file:
                self.gen_pybind_cpp_impl(mod)

    def gen_pybind_cpp_impl(self, mod: "CodeModule"):
        reg = ReflectionRegistry()
        INDENT = DEFAULT_INDENT
        print("Dependencies: [")
        extra_headers = list(mod.header_files)
        for dep in mod.deps:
            dep_mod = self._modules[dep.__name__]
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

    def gen_cpp_impl(self, mod: "CodeModule"):
        reg = ReflectionRegistry()
        INDENT = DEFAULT_INDENT

        struct_impls_list = []
        enum_initers_list = []
        
        # 从头文件路径推断接口头文件包含路径
        interface_header = self._get_interface_header_from_path(mod.cpp_interface_header)
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

    def gen_cpp_interface_header(self, mod: "CodeModule"):
        reg = ReflectionRegistry()

        print("Dependencies: [")
        extra_headers = list(mod.header_files)
        for dep in mod.deps:
            dep_mod = self._modules[dep.__name__]
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
                enums_expr.append(self.enum_gen(info))
            else:
                structs_expr.append(self.cpp_struct_def_gen(info))

        enums_expr = "\n".join(enums_expr)
        structs_expr = "\n".join(structs_expr)
        
        header_path = Path(mod.cpp_interface_header).resolve()
        file_expr = CPP_INTERFACE_TEMPLATE.substitute(
            EXTRA_INCLUDE=extra_include_expr,
            ENUMS_EXPR=enums_expr,
            STRUCTS_EXPR=structs_expr,
        )
        _write_string_to(file_expr, header_path)

    def _get_interface_header_from_path(self, header_path: str) -> str:
        """从完整路径中提取接口头文件包含路径（用于 #include）"""
        path = Path(header_path)
        parts = list(path.parts)
        if "include" in parts:
            idx = parts.index("include")
            return "/".join(parts[idx+1:])
        return header_path

    def gen_pybind_py(self, mod: "CodeModule"):
        target_filepath = mod.pybind_py_file
        print("Generating pybind to ", target_filepath)
        registry = ReflectionRegistry()
        INDENT = DEFAULT_INDENT
        type_to_cls_info = {}

        def get_class_expr(info: ClassInfo):
            if info.is_enum:
                return "", []

            struct_name = info.name
            if not info.pybind or not info.create_instance:
                init_method = PY_INIT_METHOD_TEMPLATE_EXTERNAL.substitute(INDENT=INDENT)
                dispose_method = ""
            else:
                init_method = PY_INIT_METHOD_TEMPLATE.substitute(
                    INDENT=INDENT,
                    STRUCT_NAME=struct_name,
                )

                dispose_method = PY_DISPOSE_METHOD_TEMPLATE.substitute(INDENT=INDENT)
            dispose_method += PY_BOOL_METHOD_TEMPLATE.substitute(INDENT=INDENT)
            pybind_methods_list = []
            if info.create_instance:
                pybind_methods_list.append(f"create__{struct_name}__")

            def get_method_expr(method: MethodInfo, type: Type):
                method_params = {
                    k: v for k, v in method.parameters.items() if k != "self"
                }

                args_decl = _print_py_args_decl(method_params, False, type)
                args_call = _print_py_args(method_params, False, False)

                return_expr = "return " if method.return_type else ""
                return_end = ""
                if method.return_type:
                    if hasattr(method.return_type, "_ctor_begin") or (
                        hasattr(method.return_type, "_pybind_type_")
                        and method.return_type._pybind_type_
                        and (
                            not hasattr(method.return_type, "_is_enum_")
                            or not method.return_type._is_enum_
                        )
                    ):
                        return_expr += _get_py_type(method.return_type)
                        if (
                            hasattr(method.return_type, "_ctor_begin")
                            and method.return_type._ctor_begin
                        ):
                            return_expr += "."
                            return_expr += method.return_type._ctor_begin
                        else:
                            return_expr += "("
                        if (
                            hasattr(method.return_type, "_ctor_end")
                            and method.return_type._ctor_end
                        ):
                            return_end = method.return_type._ctor_end
                        else:
                            return_end += ")"

                pybind_method_name = PYBIND_METHOD_NAME_TEMPLATE.substitute(
                    STRUCT_NAME=struct_name,
                    METHOD_NAME=method.name,
                )

                pybind_methods_list.append(pybind_method_name)
                if method.name == "dispose":
                    return PY_METHOD_DISPOSE_TEMPLATE.substitute(
                        INDENT=INDENT,
                        METHOD_NAME=method.name,
                        ARGS_DECL=args_decl,
                        RETURN_EXPR=return_expr,
                        PYBIND_METHOD_NAME=pybind_method_name,
                        ARGS_CALL=args_call,
                        RETURN_END=return_end,
                    ), pybind_methods_list
                return PY_METHOD_TEMPLATE.substitute(
                    INDENT=INDENT,
                    METHOD_NAME=method.name,
                    ARGS_DECL=args_decl,
                    RETURN_EXPR=return_expr,
                    PYBIND_METHOD_NAME=pybind_method_name,
                    ARGS_CALL=args_call,
                    RETURN_END=return_end,
                ), pybind_methods_list

            methods_list = []
            for method in info.methods:
                if method.is_inherit_func:
                    continue
                method_expr, _ = get_method_expr(method, info.cls)
                methods_list.append(method_expr)

            methods_expr = "".join(methods_list)
            inherit_expr = ""
            if len(info.base_classes) == 1:
                base_class = info.base_classes[0]
                assert base_class is not None
                base_expr = _get_py_type(base_class.cls)
                inherit_expr = f"({base_expr})"
            elif len(info.base_classes) > 1:
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

        import_reqs = []
        import_cls_reqs = []
        for cls in mod.classes:
            info = registry.get_class_info(cls.__name__)
            type_to_cls_info[info.cls] = info
            if not info.pybind:
                continue

            if info.is_enum:
                import_cls_reqs.append(info.name)

            class_expr, pybind_methods_list = get_class_expr(info)
            if class_expr:
                classes_expr_list.append(class_expr)

            import_reqs.extend(pybind_methods_list)

        import_reqs_expr = "*"
        if len(import_reqs) > 0:
            import_reqs_expr = ",".join(import_reqs)
        import_cls_reqs_expr = "*"
        if len(import_cls_reqs) > 0:
            import_cls_reqs_expr = ",".join(import_cls_reqs)

        classes_expr = "\n".join(classes_expr_list)
        enum_exprs = "\n".join(enum_exprs)

        import_module_expr = PY_MODULE_IMPORT_TEMPLATE.substitute(
            MODE_NAME=mod.name,
            PYBIND_METHODS_EXPR=import_reqs_expr,
            PYBIND_CLS_EXPR=import_cls_reqs_expr,
        )

        file_expr = PY_MODULE_TEMPLATE.substitute(
            IMPORT_MODULE_EXPR=import_module_expr,
            ENUM_EXPRS=enum_exprs,
            CLASS_EXPRS=classes_expr,
        )
        _write_string_to(file_expr, mod.pybind_py_file)

    def cpp_struct_def_gen(self, info: ClassInfo) -> str:
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

    def enum_gen(self, info: ClassInfo):
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


def codegen(cls: Optional[Type] = None, *, interface_gen=True, interface_header_path="") -> Type:
    """Codegen装饰器，用来标记Codegen任务"""

    def decorator(cls: Type) -> Type:
        r = CodegenRegistry()
        r.register(cls)
        return cls

    if cls is None:
        return decorator
    else:
        return decorator(cls)


class CodeModule:
    """
    代码生成模块基类
    
    使用指南：
    1. 指定 cpp_interface_header 路径 → 自动生成 C++ 接口头文件
    2. 指定 cpp_impl_file 路径 → 自动生成 C++ 实现文件（需要同时指定 cpp_interface_header）
    3. 指定 pybind_py_file 路径 → 自动生成 Python 绑定文件
    4. 指定 pybind_cpp_def_file 路径 → 自动生成 pybind C++ 定义文件
    
    示例：
        @codegen
        class MyModule(CodeModule):
            name = "my_module"
            cpp_interface_header = "rbc/my_module/include/my_module/generated/api.hpp"
            cpp_impl_file = "rbc/my_module/src/generated/api.cpp"
            classes = [MyClass1, MyClass2]
            deps = [OtherModule]  # 可选：依赖其他模块
    """
    # 模块名称
    name: str = "CodeModule"
    
    # 要生成的类列表
    classes: Optional[List[Type]] = None
    
    # C++ 接口头文件输出路径（指定即启用）
    cpp_interface_header: Optional[str] = None
    
    # C++ 实现文件输出路径（指定即启用，需要同时指定 cpp_interface_header）
    cpp_impl_file: Optional[str] = None
    
    # Python 绑定文件输出路径（指定即启用）
    pybind_py_file: Optional[str] = None
    
    # Pybind C++ 定义文件输出路径（指定即启用）
    pybind_cpp_def_file: Optional[str] = None
    
    # 额外头文件（用于生成的代码中包含）
    header_files: Optional[List[str]] = None
    
    # 依赖的其他 CodeModule 类
    deps: Optional[List[Type]] = None

    def __init__(self):
        # 在每个实例创建时初始化列表，确保彼此独立
        self.classes = list(self.classes) if self.classes is not None else []
        self.header_files = list(self.header_files) if self.header_files is not None else []
        self.deps = list(self.deps) if self.deps is not None else []

    def add_cls(self, cls: Type):
        self.classes.append(cls)
