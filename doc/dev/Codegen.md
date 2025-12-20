# py-first代码生成架构

## 概述

`rbc_meta` 是一个基于Python的代码生成框架，采用"Python优先"（py-first）的设计理念。通过Python类型注解和装饰器，自动生成C++接口、实现、Python绑定和RPC客户端代码。

核心思想：**在Python中定义数据结构和方法，自动生成多语言绑定代码**。

## 核心数据结构

### ClassInfo

类的完整元信息，是代码生成的基础数据结构。

```python
@dataclass
class ClassInfo:
    name: str                    # 类名
    cls: Type                    # Python类对象
    module: str                  # 所属模块名
    is_enum: bool                # 是否为枚举类型
    methods: List[MethodInfo]    # 方法列表
    fields: List[FieldInfo]      # 字段列表
    base_classes: List[str]      # 基类名称列表
    doc: Optional[str] = None    # 文档字符串
    
    # C++相关配置
    cpp_namespace: str = ""      # C++命名空间
    cpp_prefix: str = ""         # C++函数前缀（如RBC_API）
    
    # 功能开关
    serde: bool = False          # 是否启用序列化
    pybind: bool = False         # 是否生成Python绑定
```

### MethodInfo

方法信息，包含签名、类型注解等。

```python
@dataclass
class MethodInfo:
    name: str                                    # 方法名
    signature: inspect.Signature                 # 方法签名
    return_type: Optional[Type]                  # 返回类型
    parameters: Dict[str, inspect.Parameter]     # 参数字典
    return_type_generic: Optional[GenericInfo]   # 返回类型泛型信息
    parameter_generics: Dict[str, Optional[GenericInfo]]  # 参数泛型信息
    doc: Optional[str] = None                    # 文档字符串
    
    # 方法特性
    cpp_prefix: str = ""                        # C++前缀
    is_rpc: bool = False                         # 是否为RPC方法
    is_static: bool = False                      # 是否为静态方法
```

### FieldInfo

字段信息，包含类型、默认值等。

```python
@dataclass
class FieldInfo:
    name: str                          # 字段名
    type: Type                         # 字段类型
    generic_info: Optional[GenericInfo] # 泛型信息
    default: Any = None                # 默认值
    cpp_init_expr: Optional[str] = None # C++初始化表达式
    serde: Optional[bool] = None       # 是否序列化（None表示使用类级别设置）
    doc: Optional[str] = None           # 文档字符串
```

### GenericInfo

泛型类型信息，用于处理容器类型和可选类型。

```python
@dataclass
class GenericInfo:
    origin: Type                      # 泛型原始类型（如list, dict, Optional）
    args: tuple                       # 泛型参数（如(int,) 或 (str, int)）
    cpp_name: Optional[str] = None    # C++类型名称（如"std::vector"）
    is_container: bool = False        # 是否为容器类型
    is_optional: bool = False         # 是否为Optional类型
```

## 反射注册表 (ReflectionRegistry)

`ReflectionRegistry` 是单例模式的注册表，管理所有需要代码生成的类。

### 核心方法

```python
class ReflectionRegistry:
    # 单例模式
    _instance: Optional["ReflectionRegistry"] = None
    _registered_classes: Dict[str, ClassInfo] = {}
    
    def register(
        self,
        cls: Type,
        module_name: str = None,
        cpp_namespace: str = None,
        serde: bool = False,
        pybind: bool = False,
        cpp_prefix: str = "",
    ) -> Type:
        """注册类到反射系统"""
        
    def get_class_info(
        self, 
        class_name: str, 
        module_name: str = None
    ) -> Optional[ClassInfo]:
        """获取类信息"""
        
    def get_all_classes(self) -> Dict[str, ClassInfo]:
        """获取所有已注册的类"""
        
    def scan_module(self, module_name: str):
        """扫描模块，自动注册所有标记的类"""
        
    def _extract_class_info(self, cls: Type, module_name: str) -> ClassInfo:
        """提取类的元信息（方法、字段、基类等）"""
```

### 注册流程

1. **类注册**：通过 `@reflect` 装饰器或 `register()` 方法注册类
2. **信息提取**：`_extract_class_info()` 自动提取：
   - 方法信息（通过 `inspect` 模块）
   - 字段信息（通过 `get_type_hints()`）
   - 泛型信息（解析 `List[T]`, `Dict[K,V]`, `Optional[T]` 等）
   - 枚举成员（如果是枚举类型）
3. **存储**：以 `{module_name}.{class_name}` 为key存储到 `_registered_classes`

## 装饰器系统

### @reflect

类装饰器，标记需要反射的类。

```python
@reflect(
    module_name="my_module",
    cpp_namespace="my::ns",
    serde=True,      # 启用序列化
    pybind=True,     # 生成Python绑定
    cpp_prefix="RBC_API"
)
class MyClass:
    x: int
    y: float
    
    def method(self, arg: str) -> int:
        ...
```

**参数说明**：
- `module_name`: 模块名（默认使用 `cls.__module__`）
- `cpp_namespace`: C++命名空间
- `serde`: 是否启用序列化
- `pybind`: 是否生成Python绑定
- `cpp_prefix`: C++函数前缀

### @rpc

方法装饰器，标记RPC方法。

```python
@reflect
class MyService:
    @rpc
    def remote_method(self, arg: int) -> str:
        """RPC方法，会生成客户端代码"""
        ...
    
    @rpc(is_static=True)
    def static_rpc_method(arg: int) -> str:
        """静态RPC方法"""
        ...
```

**特性**：
- RPC方法会生成序列化/反序列化代码
- 支持静态RPC方法
- 自动生成客户端接口代码

### 序列化字段标记

使用 `Annotated` 类型注解标记字段的序列化行为：

```python
from typing import Annotated
from rbc_meta.utils import serde_field, no_serde_field

@reflect(serde=True)
class MyClass:
    # 明确标记需要序列化
    temperature: Annotated[float, serde_field()]
    
    # 明确标记不序列化
    internal_cache: Annotated[dict, no_serde_field()]
    
    # 使用类级别设置（默认序列化）
    name: str
```

## 代码生成系统

代码生成系统位于 `codegen.py`，提供以下生成函数：

### C++接口生成

```python
def cpp_interface_gen(
    module_filter: List[str] = None, 
    *extra_includes
) -> str:
    """生成C++接口头文件"""
```

**生成内容**：
- 枚举定义（`enum struct`）
- 结构体定义（`struct`）
- 方法声明（虚函数）
- RPC方法声明
- 序列化/反序列化声明
- RTTI元数据

### C++实现生成

```python
def cpp_impl_gen(
    module_filter: List[str] = None, 
    *extra_includes
) -> str:
    """生成C++实现文件"""
```

**生成内容**：
- 枚举初始化器
- 序列化/反序列化实现
- RPC序列化器（函数序列化器）

### Python接口生成

```python
def py_interface_gen(
    module_name: str, 
    module_filter: List[str] = None
) -> str:
    """生成Python接口代码"""
```

**生成内容**：
- Python类定义
- `__init__` 和 `__del__` 方法
- 方法包装（调用C++绑定）

### Pybind11绑定生成

```python
def pybind_codegen(
    module_name: str, 
    module_filter: List[str] = None, 
    *extra_includes
) -> str:
    """生成Pybind11绑定代码"""
```

**生成内容**：
- 枚举绑定
- 类创建/销毁函数
- 方法绑定函数
- 模块注册代码

### RPC客户端生成

```python
def cpp_client_interface_gen(
    module_filter: List[str] = None, 
    *extra_includes
) -> str:
    """生成RPC客户端接口"""
    
def cpp_client_impl_gen(
    module_filter: List[str] = None, 
    *extra_includes
) -> str:
    """生成RPC客户端实现"""
```

**生成内容**：
- 客户端类定义
- RPC调用方法（使用 `RPCCommandList`）
- 参数序列化/返回值反序列化

## 类型系统

### 内置类型

`builtin.py` 定义了内置类型映射：

```python
# 基本类型
uint -> uint32_t
ulong -> uint64_t
ubyte -> int8_t
double -> double

# 向量类型
float2 -> luisa::float2
float3 -> luisa::float3
float4 -> luisa::float4
float3x3 -> luisa::float3x3
float4x4 -> luisa::float4x4

# 容器类型
Vector[T] -> luisa::vector<T>
UnorderedMap[K, V] -> luisa::unordered_map<K, V>

# 特殊类型
GUID -> vstd::Guid
string -> luisa::string
```

### 类型映射规则

类型映射在 `codegen.py` 的 `_get_cpp_type()` 函数中实现：

1. **基本类型**：直接映射（`int` -> `int32_t`）
2. **泛型类型**：递归处理（`List[int]` -> `luisa::vector<int32_t>`）
3. **自定义类型**：检查 `_cpp_type_name` 属性
4. **命名空间**：通过 `ClassInfo.cpp_namespace` 添加命名空间前缀

### 泛型类型支持

支持的泛型类型：
- `List[T]` / `list[T]` -> `luisa::vector<T>`
- `Dict[K, V]` / `dict[K, V]` -> `luisa::unordered_map<K, V>`
- `Optional[T]` / `Union[T, None]` -> `std::optional<T>`
- `Vector[T]` -> `luisa::vector<T>`（自定义容器）
- `UnorderedMap[K, V]` -> `luisa::unordered_map<K, V>`（自定义容器）

## 使用示例

### 基本用法

```python
from rbc_meta.utils import reflect, rpc
from rbc_meta.utils.builtin import Vector, float3

@reflect(
    cpp_namespace="rbc",
    serde=True,
    pybind=True
)
class Transform:
    position: float3
    rotation: float3
    scale: float3 = float3(1.0, 1.0, 1.0)
    
    def apply(self, point: float3) -> float3:
        """应用变换"""
        ...
    
    @rpc
    def remote_transform(self, point: float3) -> float3:
        """RPC变换方法"""
        ...
```

### 枚举类型

```python
from enum import Enum
from rbc_meta.utils import reflect

@reflect(cpp_namespace="rbc")
class RenderMode(Enum):
    Forward = 0
    Deferred = 1
    RayTracing = 2
```

### 序列化控制

```python
from typing import Annotated
from rbc_meta.utils import reflect, serde_field, no_serde_field

@reflect(serde=True)
class Config:
    # 序列化字段
    width: Annotated[int, serde_field()]
    height: Annotated[int, serde_field()]
    
    # 不序列化字段
    cache: Annotated[dict, no_serde_field()]
    
    # 使用类级别设置
    name: str
```

### 代码生成

```python
from rbc_meta.utils.codegen import (
    cpp_interface_gen,
    cpp_impl_gen,
    py_interface_gen,
    pybind_codegen
)

# 生成C++接口
cpp_header = cpp_interface_gen(
    module_filter=["my_module"],
    "#include <my_header.h>"
)

# 生成C++实现
cpp_impl = cpp_impl_gen(
    module_filter=["my_module"]
)

# 生成Python接口
py_interface = py_interface_gen(
    module_name="my_module",
    module_filter=["my_module"]
)

# 生成Pybind绑定
pybind_code = pybind_codegen(
    module_name="my_module",
    module_filter=["my_module"]
)
```

## 架构特点

1. **Python优先**：在Python中定义数据结构，自动生成多语言绑定
2. **类型安全**：充分利用Python类型注解，生成类型安全的C++代码
3. **自动化**：通过装饰器自动注册，无需手动维护注册表
4. **可扩展**：支持自定义类型映射和容器类型
5. **RPC支持**：内置RPC方法支持，自动生成客户端代码
6. **序列化**：灵活的序列化控制（类级别和字段级别）

## 文件结构

```
rbc_meta/utils/
├── __init__.py          # 模块导出
├── reflect.py           # 反射核心（ReflectionRegistry, ClassInfo等）
├── codegen.py           # 代码生成函数（C++, Python, Pybind）
├── templates.py         # 代码模板
├── generator.py         # 代码生成器类（备用）
├── builtin.py           # 内置类型定义
└── codegen_util.py      # 工具函数
```

## 模板系统

所有代码模板定义在 `templates.py` 中，使用 `string.Template` 进行字符串替换：

- `CPP_*_TEMPLATE`: C++相关模板
- `PY_*_TEMPLATE`: Python相关模板
- `PYBIND_*_TEMPLATE`: Pybind11绑定模板
- `CPP_CLIENT_*_TEMPLATE`: RPC客户端模板

模板变量使用 `${VARIABLE_NAME}` 格式，通过 `Template.substitute()` 替换。
