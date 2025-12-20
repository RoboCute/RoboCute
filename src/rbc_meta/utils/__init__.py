"""
反射和代码生成框架

使用示例:
    from codegen import reflect
    from codegen.generator import CodeGenerator

    @reflect
    class MyClass:
        def method(self, x: int) -> str:
            ...

    generator = CodeGenerator()
    python_code = generator.generate_python("MyClass")
    cpp_header = generator.generate_cpp_header("MyClass", namespace="myns")
"""

from .reflect import (
    reflect,
    rpc,
    ReflectionRegistry,
    ClassInfo,
    MethodInfo,
    FieldInfo,
    GenericInfo,
)
from .generator import CodeGenerator, PythonGenerator, CppGenerator, TypeMapper

# Builtin Types
_registry = ReflectionRegistry()
# 默认首先将Builtin扫一遍
_registry.scan_module("rbc_meta.utils.builtin")

__all__ = [
    "reflect",
    "rpc",
    "ReflectionRegistry",
    "ClassInfo",
    "MethodInfo",
    "FieldInfo",
    "GenericInfo",
    "CodeGenerator",
    "PythonGenerator",
    "CppGenerator",
    "TypeMapper",
]
