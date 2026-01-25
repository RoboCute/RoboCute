"""
反射框架核心模块
提供类注册、反射和代码生成功能
"""

import inspect
import importlib
import typing
import sys
from typing import Dict, List, Optional, Any, Type, get_type_hints, get_origin, get_args
from enum import Enum as PyEnum
from dataclasses import dataclass
from rbc_meta.utils.builtin import RBCStruct

# Annotated is available from Python 3.9+
if sys.version_info >= (3, 9):
    from typing import Annotated
else:
    # For Python 3.8 compatibility, create a dummy Annotated
    Annotated = None


@dataclass
class GenericInfo:
    """泛型信息"""

    origin: Type  # 泛型原始类型，如 list, dict, Optional
    args: tuple  # 泛型参数，如 (int,) 或 (str, int)
    cpp_name: Optional[str] = (
        None  # C++类型名称，如 "std::vector", "std::unordered_map"
    )
    is_container: bool = False  # 是否为容器类型
    is_optional: bool = False  # 是否为Optional类型
    is_pointer: bool = False  # 是否为裸指针

    def __post_init__(self):
        """后处理，确保args不为None"""
        if self.args is None:
            self.args = ()


@dataclass
class MethodInfo:
    """方法信息"""

    name: str
    signature: inspect.Signature
    return_type: Optional[Type]
    parameters: Dict[str, inspect.Parameter]
    return_type_generic: Optional["GenericInfo"] = None  # 返回类型的泛型信息
    parameter_generics: Dict[str, Optional["GenericInfo"]] = None  # 参数的泛型信息
    doc: Optional[str] = None

    cpp_prefix: str = ""  # sum cpp prefix info like `RBC_API`
    is_rpc: bool = False  # 是否为RPC方法
    is_static: bool = False  # 是否为静态方法
    is_inherit_func: bool = False  # 是否为继承方法

    def __post_init__(self):
        """后处理，确保字典不为None"""
        if self.parameter_generics is None:
            self.parameter_generics = {}


@dataclass
class FieldInfo:
    """字段信息"""

    name: str
    type: Type
    generic_info: Optional["GenericInfo"] = None  # 字段类型的泛型信息
    default: Any = None
    cpp_init_expr: Optional[str] = None  # C++ 初始化表达式
    serde: Optional[bool] = None  # 是否序列化，None 表示使用类级别的 serde 设置
    doc: Optional[str] = None


@dataclass
class ClassInfo:
    """类信息"""

    name: str
    cls: Type
    module: Optional[str]
    is_enum: bool
    methods: List[MethodInfo]
    fields: List[FieldInfo]
    base_classes: List[Optional["ClassInfo"]]
    doc: Optional[str] = None
    cpp_namespace: Optional[str] = ""
    cpp_prefix: Optional[str] = ""
    serde = False
    pybind = False
    create_instance = True

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

    def register(
        self,
        cls: Type,
        module_name: Optional[str] = None,
        cpp_namespace: Optional[str] = None,
        serde: bool = False,
        pybind: bool = False,
        cpp_prefix: Optional[str] = "",
        create_instance: bool = True,
    ) -> Type:
        """注册类"""
        class_info = self._extract_class_info(cls, module_name)
        class_info.cpp_namespace = cpp_namespace
        class_info.serde = serde
        class_info.pybind = pybind
        class_info.create_instance = create_instance
        class_info.cpp_prefix = cpp_prefix

        key = cls.__name__
        if self._registered_classes.get(key) is not None:
            print(f"Duplicate Reflect Class {key}")

        self._registered_classes[key] = class_info
        # print(f"Registering {key} : {class_info.name}")
        # 返回原始类，不修改它
        return class_info

    def _extract_class_info(self, cls: Type, module_name: Optional[str]) -> ClassInfo:
        """提取类信息"""
        is_enum = issubclass(cls, PyEnum)

        # 提取方法信息
        methods = []
        # 遍历类的所有属性，查找方法

        for name in dir(cls):
            if name.startswith("_"):  # 手动定义的built-in
                continue

            attr = getattr(cls, name, None)

            if attr is None:
                continue

            if inspect.isbuiltin(attr):
                continue

            # 检查是否是方法（函数、绑定方法、静态方法、类方法）
            is_static_method = isinstance(attr, staticmethod)
            is_class_method = isinstance(attr, classmethod)
            is_function = inspect.isfunction(attr)
            is_method = inspect.ismethod(attr)

            if not (is_static_method or is_class_method or is_function or is_method):
                continue

            try:
                # 获取原始函数
                if is_static_method:
                    func = attr.__func__
                    is_static = True
                elif is_class_method:
                    func = attr.__func__
                    is_static = False  # 类方法不是静态方法
                elif is_function:
                    func = attr
                    # 函数可能是静态方法（没有self参数）
                    sig = inspect.signature(func)
                    parameters = {name: param for name, param in sig.parameters.items()}
                    is_static = len(parameters) == 0 or "self" not in parameters
                else:  # is_method
                    func = attr.__func__ if hasattr(attr, "__func__") else attr

                    # print("Method: ", func)
                    is_static = False

                is_inherit_func = False
                func_base = func.__qualname__.split(".")[0]
                if str(func_base) != str(cls.__name__):
                    # print(f"Get Inherit: {func.__qualname__}, {cls.__name__}")
                    is_inherit_func = True

                sig = inspect.signature(func)
                return_type = (
                    sig.return_annotation
                    if sig.return_annotation != inspect.Signature.empty
                    else None
                )

                # 解析返回类型的泛型信息
                return_type_generic = None
                if return_type is not None:
                    return_type_generic = self._parse_generic_type(return_type)

                parameters = {name: param for name, param in sig.parameters.items()}

                # 解析参数的泛型信息
                parameter_generics = {}
                for param_name, param in parameters.items():
                    if param.annotation != inspect.Signature.empty:
                        param_generic = self._parse_generic_type(param.annotation)
                        parameter_generics[param_name] = param_generic
                    else:
                        parameter_generics[param_name] = None

                # 检查是否为RPC方法（通过装饰器标记）
                is_rpc = hasattr(func, "_rpc_") and getattr(func, "_rpc_", False)

                # 如果通过@rpc装饰器标记了is_static，使用装饰器的设置
                if hasattr(func, "_static_"):
                    is_static = getattr(func, "_static_", is_static)

                methods.append(
                    MethodInfo(
                        name=name,
                        signature=sig,
                        return_type=return_type,
                        parameters=parameters,
                        return_type_generic=return_type_generic,
                        parameter_generics=parameter_generics,
                        doc=inspect.getdoc(func),
                        is_rpc=is_rpc,
                        is_static=is_static,
                        is_inherit_func=is_inherit_func,
                    )
                )
            except (ValueError, TypeError) as e:
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
                # 使用 include_extras=True 来保留 Annotated 类型信息
                hints = get_type_hints(cls, include_extras=True)

                # 获取 C++ 初始化表达式字典（如果存在）
                cpp_init_dict = getattr(cls, "_cpp_init", {})
                # 获取字段级别的 serde 设置字典（如果存在）
                serde_fields = getattr(cls, "_serde_fields", set())

                for name, type_hint in hints.items():
                    default = None
                    if hasattr(cls, name):
                        default = getattr(cls, name, None)

                    # 获取 C++ 初始化表达式
                    cpp_init_expr = cpp_init_dict.get(name)

                    # 检查类型注解中是否有 serde 标记（优先使用注解标记）
                    field_serde = _is_serde_field_annotation(type_hint)

                    # 如果没有注解标记，检查 _serde_fields 集合（向后兼容）
                    if field_serde is None:
                        if name in serde_fields:
                            field_serde = True
                        elif hasattr(cls, "_non_serde_fields") and name in getattr(
                            cls, "_non_serde_fields", set()
                        ):
                            field_serde = False
                        # 如果类定义了 _serde_fields 集合（非空），则不在集合中的字段默认不序列化
                        elif len(serde_fields) > 0:
                            field_serde = False

                    # 提取实际类型（如果是 Annotated，提取内部类型）
                    actual_type = _extract_annotated_type(type_hint)

                    # 解析字段类型的泛型信息（使用实际类型）
                    generic_info = self._parse_generic_type(actual_type)

                    fields.append(
                        FieldInfo(
                            name=name,
                            type=actual_type,  # 使用实际类型，而不是 Annotated 类型
                            generic_info=generic_info,
                            default=default,
                            cpp_init_expr=cpp_init_expr,
                            serde=field_serde,
                        )
                    )
            except (TypeError, AttributeError):
                # 某些类可能无法获取类型提示
                pass

        # 提取基类
        # print(f"extracting basics for {cls.__name__}: {cls.__bases__}")
        base_classes = []
        for base in cls.__bases__:
            if base != None:
                class_info = self.get_class_info(base.__name__)
                if class_info:
                    base_classes.append(class_info)

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

    def _parse_generic_type(self, type_hint: Any) -> Optional[GenericInfo]:
        """
        解析泛型类型，提取泛型信息

        支持的泛型类型：
        - List[T] / list[T] -> std::vector
        - Dict[K, V] / dict[K, V] -> std::map
        - Optional[T] / Union[T, None] -> std::optional
        - 自定义Generic容器（如Vector[T]）

        Args:
            type_hint: 类型提示

        Returns:
            GenericInfo对象，如果不是泛型则返回None
        """
        if type_hint is None or type_hint == inspect.Signature.empty:
            return None

        try:
            origin = get_origin(type_hint)
            args = get_args(type_hint)
        except (TypeError, AttributeError):
            # 不是泛型类型
            return None

        if origin is None:
            return None

        generic_info = GenericInfo(origin=origin, args=args)

        # 识别常见的泛型容器类型

        # List / list -> std::vector
        if origin is list or (hasattr(typing, "List") and origin is typing.List):
            generic_info.cpp_name = "std::vector"
            generic_info.is_container = True
            return generic_info

        # Dict / dict -> std::map (或 std::unordered_map)
        if origin is dict or (hasattr(typing, "Dict") and origin is typing.Dict):
            # 默认使用std::map，但可以通过检查类属性来判断是否使用unordered_map
            generic_info.cpp_name = "std::map"
            generic_info.is_container = True
            return generic_info

        # Optional / Union[T, None] -> std::optional
        if origin is typing.Union:
            non_none_args = [arg for arg in args if arg is not type(None)]  # noqa: E721
            if len(non_none_args) == 1 and len(args) == 2:
                # Optional类型
                generic_info.origin = Optional
                generic_info.args = (non_none_args[0],)
                generic_info.cpp_name = "std::optional"
                generic_info.is_optional = True
                return generic_info

        # 检查是否是自定义的Generic容器
        if hasattr(origin, "__name__"):
            origin_name = origin.__name__.lower()

            # 检查是否有_cpp_type_name属性
            if hasattr(origin, "_cpp_type_name"):
                generic_info.cpp_name = origin._cpp_type_name
                if hasattr(origin, "_is_container"):
                    generic_info.is_container = origin._is_container
                return generic_info

            # 通过名称识别常见的容器类型
            if "pointer" in origin_name:
                # special case for pointer
                generic_info.is_pointer = True

            if "vector" in origin_name or "list" in origin_name:
                generic_info.cpp_name = "std::vector"
                generic_info.is_container = True
                return generic_info

            if "map" in origin_name or "dict" in origin_name:
                if "unordered" in origin_name:
                    generic_info.cpp_name = "std::unordered_map"
                else:
                    generic_info.cpp_name = "std::map"
                generic_info.is_container = True
                return generic_info

        # 其他Generic类型，保留原始信息
        return generic_info

    def get_class_info(
        self, class_name: str, module_name: Optional[str] = None
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
                # 如果标记有reflected，则自动注册
                if hasattr(obj, "_reflected_"):
                    self.register(obj)

        except ImportError as e:
            import warnings

            warnings.warn(f"Cannot import module {module_name}: {e}", ImportWarning)


def reflect(
    cls: Optional[Type] = None,
    *,
    module_name: Optional[str] = None,
    cpp_namespace: Optional[str] = None,
    serde: bool = False,
    pybind: bool = False,
    create_instance: bool = True,
    cpp_prefix: Optional[str] = "",
) -> Type:
    """
    反射装饰器，用于标记需要反射的类

    用法:
        @reflect
        class MyClass:
            ...

        @reflect(cpp_namespace="rbc")
        class MyEnum(Enum):
            ...

        @reflect(module_name="custom.module", cpp_namespace="custom::ns")
        class MyStruct:
            ...
    """

    def decorator(cls: Type) -> Type:
        registry = ReflectionRegistry()
        class_info = registry.register(
            cls,
            module_name=module_name,
            cpp_namespace=cpp_namespace,
            serde=serde,
            pybind=pybind,
            cpp_prefix=cpp_prefix,
            create_instance=create_instance,
        )
        # 添加标记属性
        cls._reflected_ = True
        cls._is_enum_ = class_info.is_enum
        # 添加自定义属性
        cls._pybind_type_ = pybind

        cls._cpp_type_name = f"{cpp_namespace}::{cls.__name__}"
        return cls

    # 支持 @reflect 和 @reflect(...) 两种用法
    if cls is None:
        return decorator
    else:
        return decorator(cls)


def rpc(is_static: bool = False):
    """
    RPC方法装饰器，用于标记方法为RPC方法

    用法:
        @reflect
        class MyClass:
            @rpc
            def my_rpc_method(self, arg: int) -> str:
                ...

            @rpc(is_static=True)
            def my_static_rpc_method(arg: int) -> str:
                ...
    """

    def decorator(func):
        # 标记方法为RPC方法
        func._rpc_ = True
        func._static_ = is_static
        return func

    return decorator


class SerdeField:
    """
    序列化字段标记类，用于在类型注解中标记字段需要序列化

    用法:
        from typing import Annotated

        @reflect(serde=True)
        class MyClass:
            # 方式1: 使用 Annotated 标记序列化字段
            temperature: Annotated[float, SerdeField()]
            name: str  # 不序列化（如果类有 serde=True，默认序列化）

            # 方式2: 仍然可以使用 _serde_fields 集合
            # _serde_fields = {"temperature"}
    """

    pass


def serde_field():
    """
    序列化字段标记函数，返回 SerdeField 实例

    用法:
        from typing import Annotated

        @reflect(serde=True)
        class MyClass:
            temperature: Annotated[float, serde_field()]
            name: str
    """
    return SerdeField()


class NonSerdeField:
    """
    非序列化字段标记类，用于在类型注解中标记字段不需要序列化

    用法:
        from typing import Annotated

        @reflect(serde=True)
        class MyClass:
            # 序列化字段
            temperature: Annotated[float, serde_field()]
            # 明确标记不序列化的字段
            internal_cache: Annotated[dict, no_serde_field()]
    """

    pass


def no_serde_field():
    """
    非序列化字段标记函数，返回 NonSerdeField 实例

    用法:
        from typing import Annotated

        @reflect(serde=True)
        class MyClass:
            temperature: Annotated[float, serde_field()]
            internal_cache: Annotated[dict, no_serde_field()]
    """
    return NonSerdeField()


def _is_serde_field_annotation(type_hint: Any) -> Optional[bool]:
    """
    检查类型注解中是否包含 serde 标记

    Args:
        type_hint: 类型注解

    Returns:
        True: 标记为序列化
        False: 标记为不序列化
        None: 没有标记，使用类级别设置
    """
    # Python 3.8 不支持 Annotated
    if Annotated is None:
        return None

    # 检查是否是 Annotated 类型
    origin = get_origin(type_hint)
    if origin is not Annotated:
        return None

    # 获取 Annotated 的参数
    args = get_args(type_hint)
    if len(args) < 2:
        return None

    # 检查元数据中是否包含 SerdeField 或 NonSerdeField
    metadata = args[1:]
    for meta in metadata:
        # 检查是否是 SerdeField（标记为序列化）
        # 支持实例检查 (serde_field() 返回 SerdeField() 实例)
        # 也支持类本身检查 (虽然不太可能，但为了完整性)
        if isinstance(meta, SerdeField) or meta is SerdeField:
            return True
        # 检查是否是 NonSerdeField（标记为不序列化）
        if isinstance(meta, NonSerdeField) or meta is NonSerdeField:
            return False

    return None


def _extract_annotated_type(type_hint: Any) -> Type:
    """
    从 Annotated 类型注解中提取实际的类型

    Args:
        type_hint: 类型注解

    Returns:
        实际的类型，如果不是 Annotated 则返回原类型
    """
    # Python 3.8 不支持 Annotated
    if Annotated is None:
        return type_hint

    origin = get_origin(type_hint)
    if origin is Annotated:
        args = get_args(type_hint)
        if len(args) > 0:
            return args[0]
    return type_hint


def cpp_prefix(prefix: str):
    """
    C++前缀装饰器，用于标记方法为C++前缀方法
    """

    def decorator(func):
        func._cpp_prefix_ = prefix
        return func

    return decorator
