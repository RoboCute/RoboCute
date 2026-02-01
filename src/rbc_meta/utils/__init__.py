from .reflect import (
    reflect,
    rpc,
    ReflectionRegistry,
    ClassInfo,
    MethodInfo,
    FieldInfo,
    GenericInfo,
)
# from .generator import PythonGenerator, CppGenerator, TypeMapper

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
]
