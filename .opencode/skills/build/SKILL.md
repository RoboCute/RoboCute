---
name: build
description: RoboCute build, packaging, and Python-to-C++ codegen workflows. Use when the user asks about building, xmake configuration, preparing tools/resources, packaging/unpacking toolchain archives, or writing @reflect codegen.
triggers:
  - file_types: [".py", ".lua"]
  - patterns:
    - "xmake"
    - "prepare"
    - "build_and_copy"
    - "pre-pack"
    - "world_interface"
    - "pipeline_settings"
    - "rbc_meta"
    - "reflect"
---

# RoboCute Build & Codegen

Covers the full build pipeline: environment preparation, xmake build rules, packaging/unpacking of toolchain archives, and Python-to-C++ reflection codegen.

## Build Overview

- **Build system**: [xmake](https://xmake.io/) 3.0.6+
- **C++ Standard**: C++20
- **Primary platform**: Windows (x64), with Linux/macOS support
- **Toolchain archives**: `build/download/*.7z` → extracted to `build/tool/`
- **Runtime resources**: `build/download/*` → copied to build output by xmake/CMake hooks

## Environment Preparation

Run once on a fresh clone (requires `uv`):

```bash
uv run prepare -y
```

This performs:

1. Git clone/pull of `thirdparty/LuisaCompute` and its recursive deps (optional).
2. Download toolchain/runtime archives from `RBC_SDK_ADDRESS` into `build/download`.
3. Extract archives:
   - `clangcxx_compiler-v<ver>-windows-x64.7z` → `build/tool/clangcxx_compiler`
   - `clangd-v<ver>-windows-x64.7z` → `build/tool/clangd`
   - `oidn-*.7z` → `build/download/oidn`
   - `render_resources-*.7z` → `build/download/render_resources`
   - `dx_sdk_*.zip` (LC SDK, Windows only) → `build/download/dx_sdk`
4. Write `build/download/file_hash.json` to skip unchanged re-extractions.
5. Write `xmake/options.json` and `xmake/options.lua` from the current Python venv.
6. Generate shader compile command scripts (`rbc/shader/*_compile.cmd`).
7. Optionally clean previous `rbc/**/generated` code.

Key source files:

- `src/rbc_build/prepare.py` — archive names, SDK addresses, platform detection
- `src/rbc_build/main.py::download_packages()` — download + extraction orchestration
- `src/rbc_build/utils.py::unzip_dir()` — `.zip` via Python `zipfile`, `.7z`/`.rar` via `7z.exe`

## Toolchain Archives: Packing & Unpacking

The custom shader compiler (`clangcxx_compiler`) and the language server (`clangd`) are shipped as platform-specific `.7z` archives and extracted under `build/tool/`.

### Archive Layout

Archive files are flat (no subdirectories) so they extract directly into the destination folder.

**`clangcxx_compiler-v<ver>-windows-x64.7z`** → extract to `build/tool/clangcxx_compiler/`

Expected contents:

| File | Purpose |
|------|---------|
| `clangcxx_compiler.exe` | LuisaCompute HLSL/C++ shader compiler front-end |
| `dxcompiler.dll` | DirectX Shader Compiler |
| `dxil.dll` | DirectX IL validator/signing |
| `luisa-backend-dx.dll` | LuisaCompute DirectX backend |
| `luisa-backend-vk.dll` | LuisaCompute Vulkan backend |
| `luisa-clangcxx.dll` | Clang/C++ integration library |
| `luisa-core.dll` | LuisaCompute core runtime |
| `luisa-runtime.dll` | LuisaCompute runtime |
| `template.txt` | Compiler template metadata |

**`clangd-v<ver>-windows-x64.7z`** → extract to `build/tool/clangd/`

Expected contents:

| File | Purpose |
|------|---------|
| `clangd.exe` | Clang language server (LLVM `<version>`) |

### Naming Convention

Platform-specific names are generated in `src/rbc_build/prepare.py`:

```python
def _to_platform_spec(name):
    return f"{name}-{PLATFORM}-{ARCH}.7z"

CLANGCXX_NAME = _to_platform_spec("clangcxx_compiler-v<version>")
CLANGD_NAME   = _to_platform_spec("clangd-v<version>")
```

So on Windows x64 the resolved names are:

- `clangcxx_compiler-v<version>-windows-x64.7z`
- `clangd-v<version>-windows-x64.7z`

### Unpacking (manual)

Use 7-Zip CLI:

```bash
# clangcxx_compiler
7z x clangcxx_compiler-v<version>-windows-x64.7z -oD:\RoboCute\build\tool\clangcxx_compiler -y

# clangd
7z x clangd-v<version>-windows-x64.7z -oD:\RoboCute\build\tool\clangd -y
```

The project also locates `7z.exe` automatically in this order:

1. `<xmake_install>\winenv\bin\7z.exe`
2. `7z` / `7za` on `PATH`
3. `C:\Program Files\7-Zip\7z.exe`
4. `C:\Program Files (x86)\7-Zip\7z.exe`

### Packing (manual)

When publishing a new tool version, create a flat `.7z` from the built binaries.

Example for `clangcxx_compiler`:

```bash
cd D:\RoboCute\build\tool\clangcxx_compiler
7z a -t7z -m0=lzma2 -mx=9 ..
..\download\clangcxx_compiler-v<version>-windows-x64.7z ^
  clangcxx_compiler.exe dxcompiler.dll dxil.dll ^
  luisa-backend-dx.dll luisa-backend-vk.dll ^
  luisa-clangcxx.dll luisa-core.dll luisa-runtime.dll ^
  template.txt
```

Example for `clangd`:

```bash
cd D:\RoboCute\build\tool\clangd
7z a -t7z -m0=lzma2 -mx=9 D:\RoboCute\build\download\clangd-v<version>-windows-x64.7z clangd.exe
```

Then upload the archive to the release asset URL configured in `src/rbc_build/prepare.py::RBC_SDK_ADDRESS` and bump the version constant there.

### Updating a Tool Version

1. Build or obtain the new tool binaries.
2. Place them in `build/tool/<tool>/`.
3. Pack a new flat `.7z` as shown above.
4. Update the version constant in `src/rbc_build/prepare.py`:
   - `CLANGCXX_NAME = "clangcxx_compiler-v<new>"`
   - `CLANGD_NAME = "clangd-v<new>"`
5. Delete `build/download/file_hash.json` (or just the tool entry) to force re-extraction.
6. Run `uv run prepare -y` to verify.

## Xmake Build System

### Key Build Options

From `xmake.lua` / `xmake/options.lua`:

| Option | Description |
|--------|-------------|
| `rbc_editor` | Enable editor build |
| `rbc_tools` | Enable tools build |
| `rbc_oidn` | Enable OIDN plugin |
| `rbc_tests` | Enable tests |
| `lc_warnings` | Set to `all` to enable warning logs |
| `lc_cpu_backend` | CPU backend |
| `lc_cuda_backend` | CUDA backend (enabled) |
| `lc_dx_backend` | DirectX backend (Windows) |
| `lc_vk_backend` | Vulkan backend (enabled) |
| `lc_enable_mimalloc` | Enable mimalloc (enabled) |
| `lc_enable_gui` | Enable GUI (enabled) |
| `lc_enable_dsl` | Enable DSL (enabled) |

### Common Commands

```bash
# First-time debug config
xmake f -m debug -c

# Release config
xmake f -m release -c

# Build all targets
xmake build

# Build specific target
xmake <target_name>

# Verbose / warnings
xmake -v
xmake -w

# Run target
xmake run <target_name> -- <args>

# Clean
xmake clean
xmake clean -a

# List targets
xmake -l
xmake show --target=<target_name>
```

### Project Structure

- `xmake.lua` — root build configuration
- `xmake/options.lua` — build options
- `xmake/py_codegen.lua` — Python codegen rules
- `xmake/option_meta.lua` — option metadata
- `xmake/interface_target.lua` — interface target definitions
- `rbc/core/` — core data structures
- `rbc/runtime/` — graphics, animation, physics, plugins
- `rbc/editor/` — editor application
- `rbc/tests/` — test targets
- `rbc/extensions/` — Python extensions

### `lc_basic_settings` Rule

Primary target configuration rule:

```lua
add_rules('lc_basic_settings', {
    project_kind = 'shared',    -- 'shared' | 'binary' | 'static'
    enable_exception = true     -- optional
})
```

Examples:

```lua
-- Shared library
 target('my_module')
    add_rules('lc_basic_settings', { project_kind = 'shared' })
    add_files('src/**.cpp')
target_end()

-- Executable
 target('my_app')
    add_rules('lc_basic_settings', { project_kind = 'binary' })
    add_files('main.cpp')
target_end()

-- Inside interface_target
local function my_impl()
    add_rules('lc_basic_settings', { project_kind = 'shared' })
    add_files('src/**.cpp')
end
interface_target('my_lib', my_interface, my_impl)
```

### Shader Compilation

The `compile_shaders` and `compile_shaders_hostgen` phony targets invoke `build/tool/clangcxx_compiler/clangcxx_compiler.exe`:

```lua
compiler_path = path.join(os.projectdir(), 'build/tool/clangcxx_compiler', compiler_path)
```

Generated command scripts:

```bash
rbc/shader/dx_compile.cmd
rbc/shader/dx_clean_compile.cmd
rbc/shader/gen_json.cmd   # -lsp mode for compile_commands.json
```

## Python-to-C++ Codegen (`@reflect`)

Defines Python-to-C++ bindings and serde structs via the `@reflect` decorator for:

- `src/rbc_meta/types/world_interface.py` — pybind11 API stubs
- `src/rbc_meta/types/resource.py` — resource metadata & pixel format enums
- `src/rbc_meta/types/pipeline_settings.py` — render pipeline settings structs

### `@reflect` Parameters

| Parameter | Used In | Description |
|-----------|---------|-------------|
| `pybind=True` | world_interface, resource enums | Generate pybind11 bindings |
| `serde=True` | resource structs, pipeline_settings | Generate serialization |
| `cpp_namespace="rbc"` | all | Target C++ namespace |
| `cpp_prefix="TEST_GRAPHICS_API"` | world_interface | C++ export macro |
| `cpp_prefix="RBC_RUNTIME_API"` | resource structs | C++ export macro |
| `create_instance=False` | world_interface base classes | Disallow Python instantiation |
| `module_name="world_interface"` | enums | Python module name for enum bindings |

### Patterns

**Enum:**

```python
from enum import Enum
from rbc_meta.utils.reflect import reflect

@reflect(cpp_namespace="rbc", pybind=True)
class LpmColorSpace(Enum):
    REC709 = 0
    P3 = 1
    REC2020 = 2
```

**pybind stub class:**

```python
@reflect(
    pybind=True,
    cpp_prefix="TEST_GRAPHICS_API",
    cpp_namespace="rbc",
    create_instance=False,  # abstract base
)
class Component(Object):
    def entity() -> Entity: ...
    def update_data() -> None: ...
```

**Serde data class:**

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

**Selective serde fields:**

```python
from typing import Annotated
from rbc_meta.utils.reflect import reflect, serde_field, no_serde_field

@reflect(cpp_namespace="rbc", serde=True)
class SkySettings:
    sky_atom: Annotated[Pointer[SkyAtmosphere], no_serde_field()]
    dirty: Annotated[bool, no_serde_field()]

    sky_angle: Annotated[float, serde_field()]
    sun_dir: Annotated[float3, serde_field()]
```

**C++ default values:**

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

**Custom type mapping:**

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

### Type Mapping

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
| `None` | `void` | return type only |
| `VoidPtr` | `void*` | |
| `GUID` | `rbc::GUID` | |
| `Vector[T]` | `luisa::vector<T>` | |
| `RC[T]` | `rbc::RC<T>` | ref-counted pointer |
| `Ref[T]` | `T&` | reference |
| `Const[T]` | `const T` | const qualifier |
| `Pointer[T]` | `T*` | raw pointer |
| `DataBuffer` | host data buffer | from `builtin` |
| `Curve` / `SkyAtmosphere` / `LCBuffer` / `LCBufferView` / `LCImage` | engine builtin types | from `builtin` |

### Registration

Every reflected class must be listed in `OUT_CLASSES` and exported:

```python
OUT_CLASSES = [
    MyEnum,
    MyStubClass,
    MySerdeStruct,
]

__all__ = ["OUT_CLASSES"]
```

### Codegen Guidelines

1. **Enums**: use `pybind=True`; add `module_name` for world-interface enums.
2. **Abstract classes**: set `create_instance=False` for `Object`, `Component`, `Entity`, `Resource`, and direct subclasses.
3. **Stub methods**: use `def method(...) -> T: ...` with full annotations; no body.
4. **Inheritance**: maintain hierarchies (`Object` → `Component`/`Entity`/`Resource`) in world interface.
5. **Serde structs**: use `serde=True`; add `cpp_prefix="RBC_RUNTIME_API"` for runtime API exposure.
6. **Defaults**: provide `_cpp_init` for structs consumed by C++ when non-zero defaults are needed.
7. **Selective serde**: use `Annotated[..., serde_field()]` / `Annotated[..., no_serde_field()]`.
8. **Add to `OUT_CLASSES`**: every `@reflect`-decorated class must be registered.

## Build Artifacts & Packaging

### `uv run scripts/build_and_copy.py [debug|release|releasedbg] [uv]`

1. Runs `xmake` (or `xmake -r` with `--rebuild`).
2. Copies `*.dll`, `*.pyd`, `*.bytes` from `build/<platform>/<arch>/<mode>` to `src/robocute/rbc_ext/_C`.
3. Copies shader build directories `shader_build_dx` and `shader_build_vk`.
4. Optionally generates Python stubs with `pybind11-stubgen` via `uvx`.

### `uv run pre-pack [mode] [stubgen]`

Same copy/stub logic as above, but assumes the build directory already exists.

### CMake Post-Build Resource Installation

`cmake/rbc_install_resources.cmake` creates target `rbc_install_resources` that runs after every build:

1. Finds `uv`.
2. Runs `cmake/rbc_install_resources_post_build.py`.
3. Extracts `build/download/test_scene_*.7z` and `build/download/render_resources-*.7z` into the output directory.
4. Copies `shader_build_dx` and `shader_build_vk`.

### Typical Full Flow

```bash
# 1. Prepare environment
uv run prepare -y

# 2. Configure and build
xmake f -m releasedbg
xmake

# 3. Install artifacts into Python package
uv run scripts/build_and_copy.py releasedbg uv
```

Or with CMake:

```bash
uv run prepare -y
cmake -B build/cmake -S . -DRBC_BUILD_EDITOR=ON
cmake --build build/cmake --config Release
```

## Adding a New Downloadable Dependency

1. Add archive name and URL to `src/rbc_build/prepare.py`.
2. Add entry to `src/rbc_build/main.py::download_packages()` with desired `unzip` destination.
3. If needed at runtime, add `after_build` / `before_build` hook in relevant `xmake.lua` to copy from `build/download/<name>` to `target:targetdir()`.
4. If it is a LuisaCompute SDK, declare an `lc_install_sdk` rule and let `find_sdk.lua` cache it under `build/.lcsdk`.

## Tips

- Use `xmake f -m debug -c` for first-time setup.
- Use `xmake -v` for verbose output.
- Configuration changes require `-c` to reconfigure.
- If `oidn.dll`, `dx_sdk`, or `render_resources` are missing at runtime, re-run `uv run prepare -y` and check `build/download`.
- Keep toolchain archives flat (no subdirectory) so `7z x` extracts directly into `build/tool/<name>`.
