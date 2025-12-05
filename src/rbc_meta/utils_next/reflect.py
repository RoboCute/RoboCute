"""
反射框架核心模块
提供类注册、反射和代码生成功能
"""

import inspect
import importlib
from typing import Dict, List, Optional, Any, Type, get_type_hints
from enum import Enum as PyEnum
from dataclasses import dataclass


@dataclass
class MethodInfo:
    """方法信息"""

    name: str
    signature: inspect.Signature
    return_type: Optional[Type]
    parameters: Dict[str, inspect.Parameter]
    doc: Optional[str] = None


@dataclass
class FieldInfo:
    """字段信息"""

    name: str
    type: Type
    default: Any = None
    doc: Optional[str] = None


@dataclass
class ClassInfo:
    """类信息"""

    name: str
    cls: Type
    module: str
    is_enum: bool
    methods: List[MethodInfo]
    fields: List[FieldInfo]
    base_classes: List[str]
    doc: Optional[str] = None

    def __post_init__(self):
        """后处理，确保列表不为None"""
        if self.methods is None:
            self.methods = []
        if self.fields is None:
            self.fields = []
        if self.base_classes is None:
            self.base_classes = []


# The Singleton the Register All RBC Builtin-Types
class ReflectionRegistry:
    """反射注册表"""

    _instance: Optional["ReflectionRegistry"] = None
    _registered_classes: Dict[str, ClassInfo] = {}

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def register(self, cls: Type, module_name: str = None) -> Type:
        """注册类"""
        # print(f"registering {cls} with module name {module_name}")
        if module_name is None:
            module_name = cls.__module__

        class_info = self._extract_class_info(cls, module_name)
        key = f"{module_name}.{cls.__name__}"
        self._registered_classes[key] = class_info

        # 返回原始类，不修改它
        return cls

    def _extract_class_info(self, cls: Type, module_name: str) -> ClassInfo:
        """提取类信息"""
        is_enum = issubclass(cls, PyEnum)

        # 提取方法信息
        methods = []
        for name, method in inspect.getmembers(cls, predicate=inspect.isfunction):
            if not name.startswith("_"):
                try:
                    sig = inspect.signature(method)
                    return_type = (
                        sig.return_annotation
                        if sig.return_annotation != inspect.Signature.empty
                        else None
                    )
                    parameters = {name: param for name, param in sig.parameters.items()}
                    methods.append(
                        MethodInfo(
                            name=name,
                            signature=sig,
                            return_type=return_type,
                            parameters=parameters,
                            doc=inspect.getdoc(method),
                        )
                    )
                except (ValueError, TypeError):
                    # 某些方法可能无法获取签名
                    pass

        # 提取字段信息
        fields = []
        if is_enum:
            # Enum的特殊处理
            for member_name, member_value in cls.__members__.items():
                # 尝试获取类型注解
                member_type = (
                    type(member_value.value)
                    if hasattr(member_value, "value")
                    else type(member_value)
                )
                fields.append(
                    FieldInfo(
                        name=member_name,
                        type=member_type,
                        default=member_value.value
                        if hasattr(member_value, "value")
                        else member_value,
                    )
                )
        else:
            # 普通类的字段提取
            try:
                hints = get_type_hints(cls)

                for name, type_hint in hints.items():
                    default = None
                    if hasattr(cls, name):
                        default = getattr(cls, name, None)

                    fields.append(FieldInfo(name=name, type=type_hint, default=default))
            except (TypeError, AttributeError):
                # 某些类可能无法获取类型提示
                pass

        # 提取基类
        base_classes = [base.__name__ for base in cls.__bases__ if base != object]

        return ClassInfo(
            name=cls.__name__,
            cls=cls,
            module=module_name,
            is_enum=is_enum,
            methods=methods,
            fields=fields,
            base_classes=base_classes,
            doc=inspect.getdoc(cls),
        )

    def get_class_info(
        self, class_name: str, module_name: str = None
    ) -> Optional[ClassInfo]:
        """获取类信息"""
        if module_name:
            key = f"{module_name}.{class_name}"
        else:
            # 搜索所有已注册的类
            for key, info in self._registered_classes.items():
                if info.name == class_name:
                    return info
            return None

        return self._registered_classes.get(key)

    def get_all_classes(self) -> Dict[str, ClassInfo]:
        """获取所有已注册的类"""
        return self._registered_classes.copy()

    def scan_module(self, module_name: str):
        """扫描模块，注册所有标记的类"""
        # print(f"Scanning Module {module_name}")
        try:
            module = importlib.import_module(module_name)
            for name, obj in inspect.getmembers(module, predicate=inspect.isclass):
                # print(f"Scanning {name}")
                # 检查是否有标记属性
                if hasattr(obj, "__reflected__"):
                    self.register(obj, module_name)
        except ImportError as e:
            import warnings

            warnings.warn(f"Cannot import module {module_name}: {e}", ImportWarning)


def reflect(cls: Type) -> Type:
    """
    反射装饰器，用于标记需要反射的类

    用法:
        @reflect
        class MyClass:
            ...

        @reflect
        class MyEnum(Enum):
            ...
    """
    registry = ReflectionRegistry()
    registry.register(cls)
    # 添加标记属性
    cls.__reflected__ = True
    return cls
