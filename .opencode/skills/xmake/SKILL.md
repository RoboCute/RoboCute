---
name: xmake
---

# Xmake Build System

This project uses [xmake](https://xmake.io/) as the build system.

## Project Overview

- **Xmake Version**: 3.0.6+
- **C++ Standard**: C++20
- **Platforms**: Windows (primary), Linux, macOS

## Key Build Options

From `xmake.lua`:
- `rbc_editor`: Enable editor build
- `rbc_tools`: Enable tools build
- `rbc_oidn`: Enable OIDN plugin
- `rbc_tests`: Enable tests
- `lc_warnings`: Set to `all` to enable warnings log

## Common Commands

### Configuration

```bash
# Initialize debug configuration (first time setup)
xmake f -m debug -c

# Initialize release configuration
xmake f -m release -c

# Show all configuration options
xmake f --menu
```

### Building

```bash
# Build all targets
xmake build

# Build specific target
xmake <target_name>

# Build with verbose output
xmake -v

# Build with all warnings
xmake -w
```

### Running

```bash
# Run a target
xmake run <target_name>

# Run with arguments
xmake run <target_name> -- <args>
```

### Cleaning

```bash
# Clean build artifacts
xmake clean

# Clean all including config
xmake clean -a
```

### Target Management

```bash
# List all targets
xmake -l

# Show target info
xmake show --target=<target_name>
```

## Project Structure

- `xmake.lua` - Root build configuration
- `xmake/options.lua` - Build options
- `xmake/py_codegen.lua` - Python code generation rules
- `xmake/option_meta.lua` - Option metadata
- `xmake/interface_target.lua` - Interface target definitions
- `rbc/` - Main project modules
  - `core/` - Core data structures
  - `runtime/` - Runtime features (graphics, animation, physics, plugins)
  - `editor/` - Editor application
  - `tests/` - Test targets
  - `extensions/` - Python extensions

## Backend Options (lc_options)

- `lc_cpu_backend`: CPU backend
- `lc_cuda_backend`: CUDA backend (enabled)
- `lc_dx_backend`: DirectX backend (Windows only)
- `lc_vk_backend`: Vulkan backend (enabled)
- `lc_metal_backend`: Metal backend (macOS only)
- `lc_enable_mimalloc`: Enable mimalloc (enabled)
- `lc_enable_gui`: Enable GUI (enabled)
- `lc_enable_dsl`: Enable DSL (enabled)

## Python Integration

```bash
# Build and install Python extension
uv run scripts/build_and_copy.py debug 1
```

## lc_basic_settings Rule

The `lc_basic_settings` rule is used to apply common build settings to targets. It's the primary way to configure target properties in this project.

### Usage

```lua
add_rules('lc_basic_settings', {
    project_kind = 'shared',    -- Target type: 'shared', 'binary', or 'static'
    enable_exception = true     -- Optional: enable C++ exceptions
})
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `project_kind` | string | Target type: `'shared'` (DLL), `'binary'` (executable), or `'static'` (static lib) |
| `enable_exception` | boolean | Whether to enable C++ exception handling (default: false) |

### Examples

**Shared library (DLL):**
```lua
target('my_module')
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_files('src/**.cpp')
target_end()
```

**Executable:**
```lua
target('my_app')
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    add_files('main.cpp')
target_end()
```

**With exceptions enabled:**
```lua
target('my_module')
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true
    })
    add_files('src/**.cpp')
target_end()
```

**Within interface_target:**
```lua
local function my_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_files('src/**.cpp')
end
interface_target('my_lib', my_interface, my_impl)
```

## Download, Install, and Unpack Workflow

This project separates *preparing the environment* (downloading prebuilt tools and resources), *installing dependencies into the build tree*, and *packaging/installing build artifacts* for distribution. The relevant paths are:

| Path | Purpose |
|------|---------|
| `build/download` | Raw downloaded archives and extracted resources |
| `build/tool` | Downloaded toolchain binaries (compiler, clangd) |
| `build/.lcsdk` | Cache metadata for LuisaCompute SDK downloads |

### Environment Preparation (`uv run prepare`)

The `prepare` command (`src/rbc_build/main.py::prepare`) downloads required tooling and resources before the first build.

Workflow:
1. Optionally clones/updates git submodules (e.g. `thirdparty/LuisaCompute`).
2. Downloads archives from `RBC_SDK_ADDRESS` / `LC_SDK_ADDRESS` into `build/download`.
3. Extracts each archive:
   - `clangcxx_compiler` → `build/tool/clangcxx_compiler`
   - `clangd` → `build/tool/clangd`
   - `oidn` archive → `build/download/oidn`
   - `render_resources` archive → `build/download/render_resources`
   - `dx_sdk` archive → `build/download/dx_sdk` (LC SDK only)
4. Writes/updates `build/download/file_hash.json` to avoid re-extracting unchanged archives.

Key code in `src/rbc_build/main.py`:

```python
def download_packages():
    downloads = {
        CLANGCXX_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / CLANGCXX_NAME, tool_path / "clangcxx_compiler"],
        },
        CLANGD_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / CLANGD_NAME, tool_path / "clangd"],
        },
        OIDN_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [download_path / OIDN_NAME, download_path / "oidn"],
        },
        RENDER_RESOURCE_NAME: {
            "address": address,
            "path": download_path,
            "unzip": [
                download_path / RENDER_RESOURCE_NAME,
                download_path / "render_resources",
            ],
        },
    }
    if LC_DX_SDK:
        downloads[LC_DX_SDK] = {
            "address": lc_address,
            "path": download_path,
            "unzip": [download_path / LC_DX_SDK, download_path / "dx_sdk"],
        }
```

### LuisaCompute SDK Install (`build/.lcsdk`)

LuisaCompute provides its own SDK download rule in `thirdparty/LuisaCompute/scripts/find_sdk.lua`. It is invoked from xmake targets that depend on SDK binaries.

Workflow:
1. The `lc_install_sdk` rule calls `find_sdk.on_install_sdk(target, 'lc_install_sdk')` during `before_build`.
2. It resolves `sdk_dir` from the rule's `sdk_dir` option or the `lc_sdk_dir` config, defaulting to `thirdparty/LuisaCompute/SDKs`.
3. For each SDK listed in `libnames`, it downloads the archive if not already cached.
4. It extracts the archive to `thirdparty/LuisaCompute/SDKs/<basename>` (or the configured `extract_dir`).
5. It stores cache metadata in `build/.lcsdk/<host>/<arch>/<sdk_name>.ini` including the archive sha256, so later runs can skip unchanged downloads.
6. If `copy_dir` is set (empty string means `target:targetdir()`), it copies the extracted SDK contents into the build output directory.

Example rule usage (inside `thirdparty/LuisaCompute`):

```lua
add_rules('lc_install_sdk', {
    sdk_dir = 'thirdparty/LuisaCompute/SDKs',
    libnames = {{
        name = 'my_sdk.zip',
        extract_dir = path.join(os.projectdir(), 'build/download/my_sdk'),
        copy_dir = '',   -- empty means copy to target:targetdir()
        plat_spec = true -- transform name to my_sdk-<host>-<arch>.zip
    }}
})
```

### Xmake Build-Time Installation

Several RBC targets copy downloaded resources into the build output directory during the build.

`rbc/render_plugin/xmake.lua`:

```lua
after_build(function(target)
    local copy_opts = {
        copy_if_different = true,
        async = true,
        detach = true
    }
    os.cp(path.join(os.projectdir(), 'build/download/render_resources/*'), target:targetdir(), copy_opts)
    local dx_sdk_srcdir = path.join(os.projectdir(), 'build/download/dx_sdk')
    if os.exists(dx_sdk_srcdir) then
        os.cp(path.join(dx_sdk_srcdir, '*'), target:targetdir(), copy_opts)
    end
end)
```

`rbc/oidn_plugin/xmake.lua`:

```lua
before_build(function(target)
    os.mkdir(target:targetdir())
    os.cp(path.join(os.projectdir(), 'build/download/oidn/*'), target:targetdir(), {
        copy_if_different = true,
        async = true,
        detach = true
    })
end)
```

`rbc/tools.lua` consumes the downloaded compiler:

```lua
compiler_path = path.join(os.projectdir(), 'build/tool/clangcxx_compiler', compiler_path)
```

### Python Packaging / Installation

After xmake builds the binaries, two Python scripts install the artifacts into the Python package tree.

`uv run scripts/build_and_copy.py [debug|release|releasedbg] [uv]`:
1. Runs `xmake` (or `xmake -r` with `--rebuild`).
2. Copies `*.dll`, `*.pyd`, `*.bytes` from `build/<platform>/<arch>/<mode>` to `src/robocute/rbc_ext/_C`.
3. Copies shader build directories `shader_build_dx` and `shader_build_vk`.
4. Optionally generates Python stubs with `pybind11-stubgen`.

`uv run pre-pack [mode] [stubgen]` (alias for `src/rbc_build/main.py::pre_pack`):
- Same copy/stub logic as above, but assumes the build directory already exists.

### CMake Post-Build Resource Installation

When building through CMake, `cmake/rbc_install_resources.cmake` creates a custom target `rbc_install_resources` that runs after every build:

1. Finds the `uv` executable.
2. Runs `cmake/rbc_install_resources_post_build.py` via a wrapper script.
3. The Python script:
   - Extracts `build/download/test_scene_*.7z` into the output directory.
   - Extracts `build/download/render_resources-*.7z` into the output directory.
   - Copies `shader_build_dx` and `shader_build_vk` into the output directory.

### Typical Pack / Install / Unpack Flow

For a fresh machine the usual order is:

```bash
# 1. Prepare environment (download tools & resources)
uv run prepare -y

# 2. Configure and build with xmake
xmake f -m releasedbg
xmake

# 3. Install artifacts into the Python package
uv run scripts/build_and_copy.py releasedbg uv
```

Or with CMake:

```bash
# 1. Prepare environment
uv run prepare -y

# 2. Configure with CMake (downloads/extracts SDKs on first build)
cmake -B build/cmake -S . -DRBC_BUILD_EDITOR=ON

# 3. Build (also triggers rbc_install_resources for 7z/shader copy)
cmake --build build/cmake --config Release
```

When adding a new downloadable dependency, follow the existing pattern:
1. Add the archive name and URL to `src/rbc_build/prepare.py`.
2. Add an entry to the `downloads` table in `src/rbc_build/main.py::download_packages` with the desired `unzip` destination.
3. If the resource must be present at runtime, add an `after_build` / `before_build` hook in the relevant `xmake.lua` to copy from `build/download/<name>` to `target:targetdir()`.
4. If the dependency is a LuisaCompute SDK, declare a `lc_install_sdk` rule and let `find_sdk.lua` cache it under `build/.lcsdk`.

## Tips

- Use `xmake f -m debug -c` for the first-time setup
- Use `xmake -v` for verbose output when debugging build issues
- Configuration changes require `-c` flag to reconfigure
- If a target fails at runtime because `oidn.dll`, `dx_sdk`, or `render_resources` are missing, re-run `uv run prepare -y` and verify that `build/download` contains the extracted files
