from typing import Dict, List, Optional, Any, Type, get_type_hints, get_origin, get_args
from rbc_meta.types.resource_meta import MeshMeta, TextureMeta
from rbc_meta.utils.reflect import ReflectionRegistry


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
    PYBIND_DISPOSE_FUNC_TEMPLATE,
    PYBIND_METHOD_FUNC_TEMPLATE,
)
from rbc_meta.utils.codegen import (
    _get_full_cpp_type,
    _is_rpc_method,
    _print_arg_vars_decl,
    _get_rpc_methods,
    _print_rpc_serializer,
)
from rbc_meta.utils.codegen_util import _write_string_to
import hashlib
from pathlib import Path


from rbc_meta.types.pipeline_settings import (
    ToneMappingParameters,
    LpmColorSpace,
    ResourceColorSpace,
    LpmDisplayMode,
    NRD_CheckerboardMode,
    NRD_HitDistanceReconstructionMode,
    DistortionSettings,
    LpmDispatchParameters,
    FrameSettings,
    ACESParameters,
    ExposureSettings,
    PathTracerSettings,
    ToneMappingSettings,
    DisplaySettings,
    SkySettings,
)


def to_include_expr(x):
    return f"#include <{x}>"


class CodegenResitry:
    _instance: Optional["CodegenResitry"] = None
    _modules: Dict[str, "CodeModule"] = {}

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def register(self, cls: Type) -> Type:
        # register a module instance once
        cls_inst = cls()
        cls_inst.set_name(cls.__name__)
        self._modules[cls.__name__] = cls_inst
        return cls

    def generate(self):
        print(f"Generating {len(self._modules)} modules")

        for name, mod in self._modules.items():
            print("=========================")
            print(mod.name())
            if mod.enable_cpp_interface_:
                self.gen_interface_header(mod)
                if mod.enable_cpp_impl_:
                    self.gen_cpp_impl(mod)

    def gen_cpp_impl(self, mod: "CodeModule"):
        reg = ReflectionRegistry()
        INDENT = DEFAULT_INDENT

        struct_impls_list = []
        enum_initers_list = []
        extra_includes_expr = to_include_expr(mod.interface_header_file_)

        for cls in mod.classes_:
            print(f"Generating {cls.__name__}")
            info = reg.get_class_info(cls.__name__)
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
            rpc_serializer = _print_rpc_serializer(info, reg)
            if rpc_serializer:
                struct_impls_list.append(rpc_serializer)

        struct_impls_expr = "\n".join(struct_impls_list)
        enum_initers_expr = "\n".join(enum_initers_list)

        file_expr = CPP_IMPL_TEMPLATE.substitute(
            EXTRA_INCLUDES=extra_includes_expr,
            ENUM_INITERS_EXPR=enum_initers_expr,
            STRUCT_IMPLS_EXPR=struct_impls_expr,
        )
        cpp_path = Path(mod.cpp_base_dir_ + "/src/" + mod.cpp_impl_file_).resolve()

        _write_string_to(file_expr, cpp_path)

    def gen_interface_header(self, mod: "CodeModule"):
        reg = ReflectionRegistry()

        print("Dependencies: [")
        extra_headers = mod.header_files_

        for dep in mod.deps_:
            dep_mod = self._modules[dep.__name__]
            print("- " + dep_mod.name())
            extra_headers.extend(dep_mod.header_files_)
        print("]")

        print(f"Collected {len(extra_headers)} Header Files")
        for header in extra_headers:
            print("- " + header)

        structs_expr = []
        enums_expr = []
        extra_include_expr = "\n".join([to_include_expr(x) for x in extra_headers])

        for cls in mod.classes_:
            print(f"Generating {cls.__name__}")
            info = reg.get_class_info(cls.__name__)
            # print(cls_info)
            if info.is_enum:
                enums_expr.append(self.enum_gen(info))
            else:
                structs_expr.append(self.cpp_struct_def_gen(info))
        enums_expr = "\n".join(enums_expr)
        structs_expr = "\n".join(structs_expr)
        header_path = Path(
            mod.cpp_base_dir_ + "/include/", mod.interface_header_file_
        ).resolve()
        file_expr = CPP_INTERFACE_TEMPLATE.substitute(
            EXTRA_INCLUDE=extra_include_expr,
            ENUMS_EXPR=enums_expr,
            STRUCTS_EXPR=structs_expr,
        )
        _write_string_to(file_expr, header_path)

    def cpp_struct_def_gen(self, info: ClassInfo) -> str:
        INDENT = DEFAULT_INDENT
        registry = ReflectionRegistry()
        namespace_name = info.cpp_namespace
        class_name = info.name
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


def codegen(
    cls: Optional[Type] = None, *, interface_gen=True, interface_header_path=""
) -> Type:
    """
    Codegen装饰器，用来标记Codegen任务
    """

    def decorator(cls: Type) -> Type:
        r = CodegenResitry()
        r.register(cls)
        return cls

    if cls is None:
        return decorator
    else:
        return decorator(cls)


class CodeModule:
    # Override Method
    name_: str = "CodeModule"
    classes_: List[str] = []
    enable_cpp_interface_: bool = False
    cpp_base_dir_: str = ""
    interface_header_file_: str = ""
    enable_cpp_impl_: bool = False
    cpp_impl_file_: str = ""
    header_files_: List[str] = []
    deps_: List[Type] = []

    def set_name(self, name):
        self.name_ = name

    def name(self):
        return self.name_

    def cpp_interface_gen(self):
        if not self.enable_cpp_interface_:
            return

    def add_cls(self, cls: Type):
        self.classes_.append(cls)


@codegen
class LuisaResourceModule(CodeModule):
    header_files_ = [
        "luisa/runtime/image.h",
        "luisa/runtime/buffer.h",
        "luisa/runtime/rhi/pixel.h",
    ]


@codegen
class RBCCoreModule(CodeModule):
    header_files_ = ["rbc_core/utils/curve.h"]


@codegen
class ResourceMetaModule(CodeModule):
    interface_header_file_ = (
        "rbc/runtime/include/rbc_plugin/generated/resource_meta_x.hpp"
    )
    cpp_impl_file_ = "rbc/runtime/src/generated/resource_meta_x.cpp"
    deps_ = [LuisaResourceModule]
    classes_ = [MeshMeta, TextureMeta]


@codegen
class PipelineSettingModule(CodeModule):
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/render_plugin/"
    interface_header_file_ = "rbc_render/generated/pipeline_settings_x.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/pipeline_settings_x.cpp"
    header_files_ = ["rbc_render/procedural/sky_atmosphere.h"]
    deps_ = [LuisaResourceModule, RBCCoreModule]
    classes_ = [
        ToneMappingParameters,
        LpmColorSpace,
        ResourceColorSpace,
        LpmDisplayMode,
        NRD_CheckerboardMode,
        NRD_HitDistanceReconstructionMode,
        DistortionSettings,
        LpmDispatchParameters,
        FrameSettings,
        ACESParameters,
        ExposureSettings,
        PathTracerSettings,
        ToneMappingSettings,
        DisplaySettings,
        SkySettings,
    ]


def generate_registered():
    r = CodegenResitry()
    r.generate()
