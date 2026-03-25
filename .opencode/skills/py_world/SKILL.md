---
name: py_world
description: How to write Python world interface classes in RoboCute
triggers:
  - file_types: [".py"]
  - patterns:
    - "world_interface"
    - "rbc_meta"
---

# Python World Interface Skill

This guide explains how to define Python world interface classes for the RoboCute engine using the `@reflect` decorator.

## Overview

World interfaces are Python stub classes that define the API contract between Python and C++. They use the `@reflect` decorator to generate pybind11 bindings automatically.

## File Location

```
src/rbc_meta/types/world_interface.py
```

## Basic Structure

### 1. Required Imports

```python
from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import (
    uint, uint2, uint3, uint4, ulong,
    float2, float3, float4, float4x4,
    double, double3, double4x4,
    VoidPtr, GUID, Vector, RC, RCBase, Ref, Const, long
)
from rbc_meta.types.resource import LCPixelStorage
from enum import Enum
```

### 2. The @reflect Decorator

The `@reflect` decorator marks classes for automatic pybind11 code generation:

```python
@reflect(
    pybind=True,                    # Generate Python bindings
    cpp_prefix="TEST_GRAPHICS_API", # C++ API export macro
    cpp_namespace="rbc",            # C++ namespace
    create_instance=True,           # Allow Python instantiation (default: True)
    module_name="world_interface"   # Python module name (for Enums)
)
class MyClass:
    ...
```

**Decorator Parameters:**
- `pybind=True` - Enable Python binding generation (required)
- `cpp_prefix` - C++ API export macro (e.g., `TEST_GRAPHICS_API`)
- `cpp_namespace` - Target C++ namespace (e.g., `rbc`)
- `create_instance` - Allow Python to create instances (default: True, set False for abstract/base classes)
- `module_name` - Python module name (mainly for Enums)

## Class Patterns

### Enum Definition

```python
@reflect(cpp_namespace='rbc', module_name='world_interface', pybind=True)
class BasicDataType(Enum):
    INT = 0
    DOUBLE = 1
    STRING = 2
    BOOL = 3
    NONE = 4
```

### Base Object Class (Abstract)

```python
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,  # Abstract base class
)
class Object:
    def guid() -> GUID: ...
    def type_name() -> str: ...
    def type_id() -> GUID: ...
    def is_type(name: str) -> bool: ...
    def base_type() -> BaseObjectType: ...
```

### Component Class (Inherits from Object)

```python
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,
)
class Component(Object):
    def entity() -> Entity: ...
    def update_data() -> None: ...
    def dispose() -> None: ...
```

### Resource Class

```python
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,
)
class Resource(Object):
    def load_status() -> ResourceLoadStatus: ...
    def load() -> None: ...
    def install() -> bool: ...
    def path() -> str: ...
    def save_to_path() -> bool: ...
    def wait_loading() -> None: ...
```

## Method Declaration Syntax

Use Python stub syntax with type annotations:

```python
def method_name(param1: Type1, param2: Type2) -> ReturnType: ...
```

### Supported Types

| Python Type | C++ Equivalent | Description |
|-------------|----------------|-------------|
| `uint` | `uint32_t` | Unsigned 32-bit integer |
| `uint2/3/4` | `luisa::uint2/3/4` | Unsigned integer vectors |
| `ulong` | `uint64_t` | Unsigned 64-bit integer |
| `long` | `int64_t` | Signed 64-bit integer |
| `float2/3/4` | `luisa::float2/3/4` | Float vectors |
| `float4x4` | `luisa::float4x4` | Float 4x4 matrix |
| `double` | `double` | Double precision float |
| `double3` | `luisa::double3` | Double vector |
| `double4x4` | `luisa::double4x4` | Double 4x4 matrix |
| `str` | `std::string` / `luisa::string` | String type |
| `bool` | `bool` | Boolean |
| `None` | `void` | Void return type |
| `VoidPtr` | `void*` | Raw void pointer |
| `GUID` | `rbc::GUID` | GUID type |
| `Vector[T]` | `luisa::vector<T>` | Dynamic array |
| `RC[T]` | `rbc::RC<T>` | Reference counted pointer |
| `Ref[T]` | `T&` | Reference |
| `Const[T]` | `const T` | Const qualifier |

### Type Composition Examples

```python
def get_materials() -> Vector[RC[MaterialResource]]: ...
def update_transform(trs: Ref[Const[float4x4]]) -> None: ...
```

## Property-like Methods

For getter/setter pairs, follow this naming convention:

```python
def position() -> double3: ...           # Getter
def set_pos(pos: double3) -> None: ...   # Setter

def fov() -> double: ...                 # Getter
def set_fov(value: double) -> None: ...  # Setter
```

## Complete Example: Transform Component

```python
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,
)
class TransformComponent(Component):
    # Getters
    def position() -> double3: ...
    def scale() -> double3: ...
    def rotation() -> float4: ...
    def trs() -> double4x4: ...
    def children_count() -> ulong: ...
    
    # Setters
    def set_pos(pos: double3, recursive: bool) -> None: ...
    def set_scale(scale: double3, recursive: bool) -> None: ...
    def set_rotation(rotation: float4, recursive: bool) -> None: ...
    def set_trs_matrix(trs: double4x4, recursive: bool) -> None: ...
    def set_trs(
        pos: double3, rotation: float4, scale: double3, recursive: bool
    ) -> None: ...
```

## Registration

All classes must be listed in `OUT_CLASSES` at the end of the file:

```python
OUT_CLASSES = [
    BasicDataType,
    ResourceLoadStatus,
    Object,
    Entity,
    Component,
    TransformComponent,
    # ... add your class here
]

__all__ = ["OUT_CLASSES"]
```

## Guidelines

1. **Use `create_instance=False`** for abstract base classes (`Object`, `Component`, `Entity`, `Resource`)
2. **Use type annotations** for all parameters and return types
3. **Use `...`** (Ellipsis) as the method body for stub methods
4. **Inherit properly** - maintain the hierarchy: `Object` → `Component`/`Entity`/`Resource`
5. **Add to `OUT_CLASSES`** - every decorated class must be registered
6. **Use snake_case** for method names (they will be bound to C++ camelCase or snake_case)
7. **Document with docstrings** for complex resources (shown in Python help)

## Common Patterns

### Entity-Component Pattern

```python
# Entity can manage components
def add_component(name: str) -> VoidPtr: ...
def get_component(name: str) -> VoidPtr: ...
def remove_component(name: str) -> bool: ...

# Component references its entity
def entity() -> Entity: ...
```

### Resource Pattern

```python
# Lifecycle methods
def load() -> None: ...
def install() -> bool: ...
def wait_loading() -> None: ...
def load_status() -> ResourceLoadStatus: ...

# Factory method
def create_empty(args...) -> None: ...
```

### Data Component Pattern

```python
def get_info(name: str) -> BasicData: ...
def set_info(name: str, data: BasicData) -> None: ...
def has_info(name: str) -> bool: ...
def bind_event(event_type: DataComponentEventType, callback_name: str) -> None: ...
```
