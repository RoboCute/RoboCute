"""
架构适配器：将旧版 type_register 结构转换为新版架构格式

这个模块提供了将旧版 API 定义的结构转换为新版架构可以使用的格式的功能。
"""

import inspect
from typing import Dict, List, Optional, Any, Type
from enum import Enum as PyEnum

try:
    import rbc_meta.utils_next.reflect as reflect_new
    import rbc_meta.utils_next.generator as generator_new
    from rbc_meta.utils_next.reflect import (
        ReflectionRegistry,
        ClassInfo,
        MethodInfo,
        FieldInfo,
    )
    from rbc_meta.utils_next.generator import CodeGenerator, TypeMapper

    _NEW_ARCH_AVAILABLE = True
except ImportError:
    _NEW_ARCH_AVAILABLE = False

import rbc_meta.utils.type_register as tr


def convert_struct_to_class_info(struct_obj: tr.struct) -> Optional["ClassInfo"]:
    """
    将旧版 struct 对象转换为新版 ClassInfo

    Args:
        struct_obj: 旧版 struct 对象

    Returns:
        ClassInfo 对象，如果新版架构不可用则返回 None
    """
    if not _NEW_ARCH_AVAILABLE:
        return None

    # 创建方法信息列表
    methods = []
    for method_name, method_overloads in struct_obj._methods.items():
        for key, func_t in method_overloads.items():
            # 将旧版参数转换为 inspect.Parameter
            parameters = {}
            for param_name, param_type in func_t._args.items():
                # 创建参数对象
                param = inspect.Parameter(
                    param_name,
                    inspect.Parameter.POSITIONAL_OR_KEYWORD,
                    annotation=_map_old_type_to_python(param_type),
                )
                parameters[param_name] = param

            # 创建方法签名
            return_annotation = None
            if func_t._ret_type:
                return_annotation = _map_old_type_to_python(func_t._ret_type)

            # 创建签名对象
            sig = inspect.Signature(
                list(parameters.values()), return_annotation=return_annotation
            )

            method_info = MethodInfo(
                name=method_name,
                signature=sig,
                return_type=return_annotation,
                parameters=parameters,
                doc=None,
            )
            methods.append(method_info)

    # 创建字段信息列表
    fields = []
    for field_name, field_type in struct_obj._members.items():
        field_info = FieldInfo(
            name=field_name,
            type=_map_old_type_to_python(field_type),
            default=struct_obj._cpp_initer.get(field_name),
            doc=None,
        )
        fields.append(field_info)

    # 创建 ClassInfo
    class_info = ClassInfo(
        name=struct_obj.class_name(),
        cls=None,  # 旧版结构没有实际的类对象
        module=struct_obj.namespace_name() or __name__,
        is_enum=False,
        methods=methods,
        fields=fields,
        base_classes=[],
        doc=struct_obj._doc,
    )

    return class_info


def convert_enum_to_class_info(enum_obj: tr.enum) -> Optional["ClassInfo"]:
    """
    将旧版 enum 对象转换为新版 ClassInfo

    Args:
        enum_obj: 旧版 enum 对象

    Returns:
        ClassInfo 对象，如果新版架构不可用则返回 None
    """
    if not _NEW_ARCH_AVAILABLE:
        return None

    # 创建字段信息（枚举成员）
    fields = []
    for name, value in enum_obj._params:
        field_info = FieldInfo(
            name=name,
            type=int if value is None else type(value),
            default=value if value is not None else name,
            doc=None,
        )
        fields.append(field_info)

    # 创建 ClassInfo
    class_info = ClassInfo(
        name=enum_obj.class_name(),
        cls=None,
        module=enum_obj.namespace_name() or __name__,
        is_enum=True,
        methods=[],
        fields=fields,
        base_classes=[],
        doc=None,
    )

    return class_info


def _map_old_type_to_python(old_type) -> Type:
    """
    将旧版类型对象映射到 Python 类型

    Args:
        old_type: 旧版类型对象（如 tr.int, tr.string 等）

    Returns:
        Python 类型
    """
    # 基本类型映射
    type_mapping = {
        tr.bool: bool,
        tr.byte: int,
        tr.ubyte: int,
        tr.short: int,
        tr.ushort: int,
        tr.int: int,
        tr.uint: int,
        tr.long: int,
        tr.ulong: int,
        tr.float: float,
        tr.double: float,
        tr.string: str,
        tr.VoidPtr: object,
        tr.DataBuffer: bytes,
        tr.void: type(None),
        # 向量类型映射为 tuple
        tr.int2: tuple,
        tr.int3: tuple,
        tr.int4: tuple,
        tr.uint2: tuple,
        tr.uint3: tuple,
        tr.uint4: tuple,
        tr.long2: tuple,
        tr.long3: tuple,
        tr.long4: tuple,
        tr.ulong2: tuple,
        tr.ulong3: tuple,
        tr.ulong4: tuple,
        tr.float2: tuple,
        tr.float3: tuple,
        tr.float4: tuple,
        tr.double2: tuple,
        tr.double3: tuple,
        tr.double4: tuple,
        tr.float2x2: tuple,
        tr.float3x3: tuple,
        tr.float4x4: tuple,
        tr.double2x2: tuple,
        tr.double3x3: tuple,
        tr.double4x4: tuple,
    }

    # 如果是模板类型
    if isinstance(old_type, tr.ClassPtr):
        return object
    elif isinstance(old_type, tr.unordered_map):
        return dict
    elif isinstance(old_type, tr.external_type):
        return object  # 外部类型映射为 object

    # 如果是 struct 或 enum
    if isinstance(old_type, tr.struct):
        return object
    if isinstance(old_type, tr.enum):
        return int

    return type_mapping.get(old_type, object)


def get_all_structs_as_class_info() -> Dict[str, "ClassInfo"]:
    """
    获取所有已注册的结构，转换为新版 ClassInfo 格式

    Returns:
        字典，键为结构全名，值为 ClassInfo 对象
    """
    result = {}
    for struct_name, struct_obj in tr._registed_struct_types.items():
        if not struct_obj._cpp_external:
            class_info = convert_struct_to_class_info(struct_obj)
            if class_info:
                result[struct_name] = class_info
    return result


def get_all_enums_as_class_info() -> Dict[str, "ClassInfo"]:
    """
    获取所有已注册的枚举，转换为新版 ClassInfo 格式

    Returns:
        字典，键为枚举全名，值为 ClassInfo 对象
    """
    result = {}
    for enum_name, enum_obj in tr._registed_enum_types.items():
        if not enum_obj._cpp_external:
            class_info = convert_enum_to_class_info(enum_obj)
            if class_info:
                result[enum_name] = class_info
    return result


def generate_code_with_new_arch(
    struct_name: Optional[str] = None,
    enum_name: Optional[str] = None,
    output_format: str = "python",
) -> str:
    """
    使用新版架构生成代码

    Args:
        struct_name: 结构名称（可选）
        enum_name: 枚举名称（可选）
        output_format: 输出格式，"python" 或 "cpp"

    Returns:
        生成的代码字符串
    """
    if not _NEW_ARCH_AVAILABLE:
        raise RuntimeError("新版架构不可用")

    generator = CodeGenerator()

    if struct_name:
        class_info = convert_struct_to_class_info(
            tr._registed_struct_types.get(struct_name)
        )
        if not class_info:
            raise ValueError(f"结构 {struct_name} 不存在或无法转换")

        if output_format == "python":
            from ..utils_next.generator import PythonGenerator

            return PythonGenerator.generate_class(class_info)
        elif output_format == "cpp":
            from ..utils_next.generator import CppGenerator

            namespace = struct_name.split("::")[0] if "::" in struct_name else None
            return CppGenerator.generate_header(class_info, namespace)

    elif enum_name:
        class_info = convert_enum_to_class_info(tr._registed_enum_types.get(enum_name))
        if not class_info:
            raise ValueError(f"枚举 {enum_name} 不存在或无法转换")

        if output_format == "python":
            from ..utils_next.generator import PythonGenerator

            return PythonGenerator.generate_class(class_info)
        elif output_format == "cpp":
            from ..utils_next.generator import CppGenerator

            namespace = enum_name.split("::")[0] if "::" in enum_name else None
            return CppGenerator.generate_header(class_info, namespace)

    else:
        raise ValueError("必须指定 struct_name 或 enum_name")

    raise ValueError(f"不支持的输出格式: {output_format}")
