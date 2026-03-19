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
xmake

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

## Tips

- Use `xmake f -m debug -c` for the first-time setup
- Use `xmake -v` for verbose output when debugging build issues
- Configuration changes require `-c` flag to reconfigure
