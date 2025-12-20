"""
代码生成器模块
支持生成Python和C++代码
"""

import inspect
import typing
from typing import Dict, List, Optional, get_origin, get_args
from .reflect import ClassInfo, MethodInfo, FieldInfo, ReflectionRegistry


class TypeMapper:
    """类型映射器，用于Python类型到C++类型的转换"""

    PYTHON_TO_CPP = {
        int: "int",
        float: "double",
        str: "std::string",
        bool: "bool",
        list: "std::vector",
        dict: "std::map",
        Optional: "std::optional",
    }

    @classmethod
    def python_to_cpp_type(cls, python_type) -> str:
        """将Python类型转换为C++类型"""
        # 处理None类型
        if python_type is type(None):
            return "void"

        # 处理Optional类型
        try:
            origin = get_origin(python_type)
            args = get_args(python_type)
        except (TypeError, AttributeError):
            origin = None
            args = ()

        # 处理Optional/Union类型
        if origin is typing.Union:
            non_none_args = [arg for arg in args if arg is not type(None)]
            if len(non_none_args) == 1 and len(args) == 2:
                # Optional类型
                inner_type = non_none_args[0]
                return f"std::optional<{cls.python_to_cpp_type(inner_type)}>"

        # 处理List类型
        if origin is list or (hasattr(typing, "List") and origin is typing.List):
            if args:
                inner_type = cls.python_to_cpp_type(args[0])
                return f"std::vector<{inner_type}>"
            return "std::vector<void*>"

        # 处理Dict类型
        if origin is dict or (hasattr(typing, "Dict") and origin is typing.Dict):
            if len(args) >= 2:
                key_type = cls.python_to_cpp_type(args[0])
                value_type = cls.python_to_cpp_type(args[1])
                return f"std::map<{key_type}, {value_type}>"
            return "std::map<std::string, void*>"

        # 基本类型映射
        if python_type in cls.PYTHON_TO_CPP:
            return cls.PYTHON_TO_CPP[python_type]

        # 如果是类，返回类名
        if hasattr(python_type, "__name__"):
            return python_type.__name__

        # 默认返回void*
        return "void*"

    @classmethod
    def format_cpp_parameter(cls, param: inspect.Parameter) -> str:
        """格式化C++参数"""
        param_type = (
            cls.python_to_cpp_type(param.annotation)
            if param.annotation != inspect.Parameter.empty
            else "void*"
        )
        default = ""
        if param.default != inspect.Parameter.empty:
            if isinstance(param.default, str):
                default = f' = "{param.default}"'
            elif isinstance(param.default, (int, float, bool)):
                default = f" = {param.default}"
            elif param.default is None:
                default = " = nullptr"
        return f"{param_type} {param.name}{default}"


class PythonGenerator:
    """Python代码生成器"""

    @staticmethod
    def generate_class(info: ClassInfo, include_doc: bool = True) -> str:
        """生成Python类代码"""
        lines = []

        # 类文档
        if include_doc and info.doc:
            lines.append(f'    """{info.doc}"""')

        # 基类
        base_str = ""
        if info.base_classes:
            base_str = f"({', '.join(info.base_classes)})"
        elif info.is_enum:
            base_str = "(Enum)"

        lines.append(f"class {info.name}{base_str}:")

        # 字段
        if info.fields:
            for field in info.fields:
                if info.is_enum:
                    # Enum成员
                    if isinstance(field.default, str):
                        lines.append(f'    {field.name} = "{field.default}"')
                    else:
                        lines.append(f"    {field.name} = {field.default}")
                else:
                    # 类字段
                    type_str = (
                        field.type.__name__
                        if hasattr(field.type, "__name__")
                        else str(field.type)
                    )
                    default_str = ""
                    if field.default is not None:
                        if isinstance(field.default, str):
                            default_str = f' = "{field.default}"'
                        else:
                            default_str = f" = {field.default}"
                    lines.append(f"    {field.name}: {type_str}{default_str}")

        # 方法
        for method in info.methods:
            lines.append("")
            if include_doc and method.doc:
                lines.append(
                    f"    def {method.name}(self{PythonGenerator._format_python_signature(method)}):"
                )
                lines.append(f'        """{method.doc}"""')
                lines.append("        ...")
            else:
                lines.append(
                    f"    def {method.name}(self{PythonGenerator._format_python_signature(method)}):"
                )
                lines.append("        ...")

        return "\n".join(lines)

    @staticmethod
    def _format_python_signature(method: MethodInfo) -> str:
        """格式化Python方法签名"""
        params = []
        for name, param in method.parameters.items():
            if name == "self":
                continue

            param_str = name
            if param.annotation != inspect.Parameter.empty:
                type_str = (
                    param.annotation.__name__
                    if hasattr(param.annotation, "__name__")
                    else str(param.annotation)
                )
                param_str += f": {type_str}"

            if param.default != inspect.Parameter.empty:
                if isinstance(param.default, str):
                    param_str += f' = "{param.default}"'
                else:
                    param_str += f" = {param.default}"

            params.append(param_str)

        return_type = ""
        if method.return_type and method.return_type != inspect.Signature.empty:
            return_type_str = (
                method.return_type.__name__
                if hasattr(method.return_type, "__name__")
                else str(method.return_type)
            )
            return_type = f" -> {return_type_str}"

        return f"({', '.join(params)}){return_type}"


class CppGenerator:
    """C++代码生成器"""

    @staticmethod
    def generate_header(info: ClassInfo, namespace: Optional[str] = None) -> str:
        """生成C++头文件"""
        lines = []
        lines.append("#pragma once")
        lines.append("")
        lines.append("#include <string>")
        lines.append("#include <vector>")
        lines.append("#include <map>")
        lines.append("#include <optional>")
        if info.is_enum:
            lines.append("#include <cstdint>")
        lines.append("")

        if namespace:
            lines.append(f"namespace {namespace} {{")
            lines.append("")

        # 类文档
        if info.doc:
            lines.append(f"// {info.doc}")

        # Enum处理
        if info.is_enum:
            lines.append(f"enum class {info.name} : int {{")
            for field in info.fields:
                lines.append(f"    {field.name} = {field.default},")
            lines.append("};")
        else:
            # 类声明
            base_str = ""
            if info.base_classes:
                base_str = f" : public {info.base_classes[0]}"

            lines.append(f"class {info.name}{base_str} {{")
            lines.append("public:")

            # 字段
            for field in info.fields:
                field_type = TypeMapper.python_to_cpp_type(field.type)
                default = ""
                if field.default is not None:
                    if isinstance(field.default, str):
                        default = f' = "{field.default}"'
                    elif isinstance(field.default, (int, float, bool)):
                        default = f" = {field.default}"
                lines.append(f"    {field_type} {field.name}{default};")

            # 方法
            for method in info.methods:
                return_type = (
                    TypeMapper.python_to_cpp_type(method.return_type)
                    if method.return_type
                    else "void"
                )
                params = []
                for name, param in method.parameters.items():
                    if name == "self":
                        continue
                    params.append(TypeMapper.format_cpp_parameter(param))

                params_str = ", ".join(params)
                lines.append(f"    {return_type} {method.name}({params_str});")

            lines.append("};")

        if namespace:
            lines.append("")
            lines.append(f"}} // namespace {namespace}")

        return "\n".join(lines)

    @staticmethod
    def generate_implementation(
        info: ClassInfo, namespace: Optional[str] = None
    ) -> str:
        """生成C++实现文件"""
        header_name = info.name.lower() + ".h"
        lines = []
        lines.append(f'#include "{header_name}"')
        lines.append("")

        if namespace:
            lines.append(f"namespace {namespace} {{")
            lines.append("")

        # 生成方法实现（空实现）
        if not info.is_enum:
            for method in info.methods:
                return_type = (
                    TypeMapper.python_to_cpp_type(method.return_type)
                    if method.return_type
                    else "void"
                )
                params = []
                for name, param in method.parameters.items():
                    if name == "self":
                        continue
                    params.append(TypeMapper.format_cpp_parameter(param))

                params_str = ", ".join(params)
                return_stmt = (
                    "return {};"
                    if method.return_type and method.return_type != type(None)
                    else ""
                )
                lines.append(
                    f"{return_type} {info.name}::{method.name}({params_str}) {{"
                )
                if return_stmt:
                    lines.append(f"    {return_stmt}")
                lines.append("}")
                lines.append("")

        if namespace:
            lines.append(f"}} // namespace {namespace}")

        return "\n".join(lines)


class CodeGenerator:
    """代码生成器主类"""

    def __init__(self, registry: Optional[ReflectionRegistry] = None):
        self.registry = registry or ReflectionRegistry()
        self.python_gen = PythonGenerator()
        self.cpp_gen = CppGenerator()

    def generate_python(
        self, class_name: str, module_name: Optional[str] = None
    ) -> str:
        """生成Python代码"""
        info = self.registry.get_class_info(class_name, module_name)
        if not info:
            raise ValueError(f"Class {class_name} not found in registry")
        return self.python_gen.generate_class(info)

    def generate_cpp_header(
        self,
        class_name: str,
        module_name: Optional[str] = None,
        namespace: Optional[str] = None,
    ) -> str:
        """生成C++头文件"""
        info = self.registry.get_class_info(class_name, module_name)
        if not info:
            raise ValueError(f"Class {class_name} not found in registry")
        return self.cpp_gen.generate_header(info, namespace)

    def generate_cpp_implementation(
        self,
        class_name: str,
        module_name: Optional[str] = None,
        namespace: Optional[str] = None,
    ) -> str:
        """生成C++实现文件"""
        info = self.registry.get_class_info(class_name, module_name)
        if not info:
            raise ValueError(f"Class {class_name} not found in registry")
        return self.cpp_gen.generate_implementation(info, namespace)

    def generate_all_python(self) -> Dict[str, str]:
        """生成所有已注册类的Python代码"""
        result = {}
        for key, info in self.registry.get_all_classes().items():
            result[key] = self.python_gen.generate_class(info)
        return result

    def generate_all_cpp(self, namespace: Optional[str] = None) -> Dict[str, tuple]:
        """生成所有已注册类的C++代码（返回(header, implementation)元组）"""
        result = {}
        for key, info in self.registry.get_all_classes().items():
            header = self.cpp_gen.generate_header(info, namespace)
            impl = self.cpp_gen.generate_implementation(info, namespace)
            result[key] = (header, impl)
        return result
