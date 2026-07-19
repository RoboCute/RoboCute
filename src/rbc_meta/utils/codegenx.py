"""
代码生成框架 - 简洁版 API

核心设计：路径即意图
- 指定 cpp_interface_header → 生成 C++ 接口头文件
- 指定 cpp_impl_file → 生成 C++ 实现文件
- 指定 pybind_py_file → 生成 Python 绑定
- 指定 pybind_cpp_def_file → 生成 pybind C++ 定义
- 指定 py_schema_file → 生成自包含 Python schema（dataclass）
"""

from typing import Dict, List, Optional, Any, Type

from rbc_meta.utils.codegen_cpp import (
    gen_cpp_impl,
    gen_cpp_interface_header,
)
from rbc_meta.utils.codegen_py import gen_pybind_py
from rbc_meta.utils.codegen_pybind import gen_pybind_cpp_impl
from rbc_meta.utils.codegen_py_schema import gen_py_schema


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
            dep_mods = [self._modules[dep.__name__] for dep in mod.deps]

            # 路径即意图：指定路径即启用功能
            if mod.cpp_interface_header:
                gen_cpp_interface_header(mod, dep_mods)
                if mod.cpp_impl_file:
                    gen_cpp_impl(mod)

            if mod.pybind_py_file:
                gen_pybind_py(mod)

            if mod.pybind_cpp_def_file:
                gen_pybind_cpp_impl(mod, dep_mods)

            if mod.py_schema_file:
                gen_py_schema(mod)


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

    # 自包含 Python schema 输出路径（指定即启用）
    py_schema_file: Optional[str] = None

    # Python schema 顶层（root）类名；缺省取 classes 列表最后一个
    py_schema_root: Optional[str] = None

    # 额外常量（生成到 C++ 接口头文件尾部，namespace 取第一个类的 cpp_namespace）
    extra_constants: Optional[Dict[str, int]] = None

    # 额外头文件（用于生成的代码中包含）
    header_files: Optional[List[str]] = None

    # 依赖的其他 CodeModule 类
    deps: Optional[List[Type]] = None

    def __init__(self):
        # 在每个实例创建时初始化列表，确保彼此独立
        self.classes = list(self.classes) if self.classes is not None else []
        self.header_files = list(self.header_files) if self.header_files is not None else []
        self.deps = list(self.deps) if self.deps is not None else []
        self.extra_constants = dict(self.extra_constants) if self.extra_constants is not None else {}

    def add_cls(self, cls: Type):
        self.classes.append(cls)
