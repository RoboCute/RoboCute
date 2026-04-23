---
name: py_codegen
description: How to write Python codegen to c++ in RoboCute
triggers:
  - file_types: [".py"]
  - patterns:
    - "world_interface"
    - "pipeline_settings"
    - "rbc_meta"
---

# Python Code Generation Skill

Defines Python-to-C++ bindings and serde structs via the `@reflect` decorator for three targets:
- `src/rbc_meta/types/world_interface.py` — pybind11 API stubs
- `src/rbc_meta/types/resource.py` — resource metadata & pixel format enums
- `src/rbc_meta/types/pipeline_settings.py` — render pipeline settings structs

## @reflect Parameters

| Parameter | Used In | Description |
|-----------|---------|-------------|
| `pybind=True` | world_interface, resource enums | Generate pybind11 bindings |
| `serde=True` | resource structs, pipeline_settings | Generate serialization |
| `cpp_namespace="rbc"` | all | Target C++ namespace |
| `cpp_prefix="TEST_GRAPHICS_API"` | world_interface | C++ export macro |
| `cpp_prefix="RBC_RUNTIME_API"` | resource structs | C++ export macro |
| `create_instance=False` | world_interface base classes | Disallow Python instantiation |
| `module_name="world_interface"` | enums | Python module name for enum bindings |

## Pattern: Enum (Resource & World Interface)

```python
from enum import Enum

@reflect(cpp_namespace="rbc", pybind=True)
class LpmColorSpace(Enum):
    REC709 = 0
    P3 = 1
    REC2020 = 2
```

## Pattern: pybind Stub Class (World Interface)

```python
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,  # Abstract base
)
class Component(Object):
    def entity() -> Entity: ...
    def update_data() -> None: ...
```

## Pattern: Serde Data Class (Resource Meta / Pipeline Settings)

```python
@reflect(
    cpp_namespace="rbc",
    serde=True,
    cpp_prefix="RBC_RUNTIME_API",
)
class TextureMeta:
    width: uint
    height: uint
    storage: LCPixelStorage
    mip_level: uint
```

## Pattern: Selective Serde Fields

Use `Annotated` with `serde_field()` / `no_serde_field()` when only some members serialize:

```python
from typing import Annotated
from rbc_meta.utils.reflect import reflect, serde_field, no_serde_field

@reflect(cpp_namespace="rbc", serde=True)
class SkySettings:
    # Runtime-only (not serialized)
    sky_atom: Annotated[Pointer[SkyAtmosphere], no_serde_field()]
    dirty: Annotated[bool, no_serde_field()]

    # Serialized fields
    sky_angle: Annotated[float, serde_field()]
    sun_dir: Annotated[float3, serde_field()]
```

## Pattern: C++ Default Values

Provide `_cpp_init` dict for C++ constructor defaults:

```python
@reflect(cpp_namespace="rbc", serde=True)
class DistortionSettings:
    scale: float
    intensity: float
    center: float2

    _cpp_init = {
        "scale": "1.0",
        "intensity": "0.0",
        "center": "0.0f, 0.0f",
    }
```

## Pattern: Custom Type Mapping

For types that map to existing C++/Python types without reflection:

```python
class LCPYBuffer:
    __slot__ = {}
    _reflected_ = True
    _cpp_type_name = "luisa::compute::BufferCreationInfoInterop"

class Callback:
    __slot__ = {}
    _reflected_ = True
    _py_type_name = ""
    _cpp_arg_call = "to_cppfunc_5d4636ab<rbc::RCBase*>"

    def _cpp_type_name(py_interface: bool, is_view: bool):
        return "py::function" if py_interface else "luisa::move_only_function<void(rbc::RCBase*)>&&"
```

## Type Mapping

| Python | C++ | Notes |
|--------|-----|-------|
| `uint` | `uint32_t` | |
| `uint2/3/4` | `luisa::uint2/3/4` | |
| `ulong` | `uint64_t` | |
| `long` | `int64_t` | |
| `float2/3/4` | `luisa::float2/3/4` | |
| `float3x3` | `luisa::float3x3` | |
| `float4x4` | `luisa::float4x4` | |
| `double` | `double` | |
| `double3` | `luisa::double3` | |
| `double4x4` | `luisa::double4x4` | |
| `str` | `std::string` / `luisa::string` | |
| `bool` | `bool` | |
| `None` | `void` | Return type only |
| `VoidPtr` | `void*` | |
| `GUID` | `rbc::GUID` | |
| `Vector[T]` | `luisa::vector<T>` | |
| `RC[T]` | `rbc::RC<T>` | Ref-counted pointer |
| `Ref[T]` | `T&` | Reference |
| `Const[T]` | `const T` | Const qualifier |
| `Pointer[T]` | `T*` | Raw pointer |
| `DataBuffer` | Host data buffer | From `builtin` |
| `Curve` / `SkyAtmosphere` / `LCBuffer` / `LCBufferView` / `LCImage` | Engine builtin types | From `builtin` |

## Registration

Every reflected class must be listed in `OUT_CLASSES` and exported via `__all__`:

```python
OUT_CLASSES = [
    MyEnum,
    MyStubClass,
    MySerdeStruct,
]

__all__ = ["OUT_CLASSES"]
```

## Guidelines

1. **Enums**: Use `pybind=True` and `module_name` for world-interface enums; plain `pybind=True` for resource enums.
2. **Abstract classes**: Set `create_instance=False` for `Object`, `Component`, `Entity`, `Resource`, and their direct subclasses.
3. **Stub methods**: Use `def method(...) -> T: ...` with full type annotations; no implementation body.
4. **Inheritance**: Maintain hierarchies (`Object` → `Component`/`Entity`/`Resource`) in world interface.
5. **Serde structs**: Use `serde=True`; add `cpp_prefix="RBC_RUNTIME_API"` for runtime API exposure.
6. **Defaults**: Provide `_cpp_init` for structs consumed by C++ when you need non-zero defaults.
7. **Selective serde**: Use `Annotated[..., serde_field()]` / `Annotated[..., no_serde_field()]` for partial serialization.
8. **Add to `OUT_CLASSES`**: Every `@reflect`-decorated class must be registered or it won't generate code.
