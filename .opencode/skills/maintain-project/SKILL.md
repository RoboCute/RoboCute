---
name: maintain-project
description: Project maintenance and development workflow for RoboCute
triggers:
  - file_types: [".py", ".cpp", ".hpp", ".h", ".lua", ".md"]
  - keywords: ["build", "compile", "test", "debug", "deploy", "clean"]
---

# RoboCute Project Maintenance Skill

This skill provides comprehensive guidance for maintaining and developing the RoboCute project.

## Project Overview

RoboCute is a **Python-first** 3D AIGC/Robotics development tool with:
- Node-based workflow (similar to ComfyUI)
- Self-developed cross-platform graphics engine
- C++ runtime with Qt6-based editor
- Python server for logic and AI integration

## Project Structure

```
RoboCute/
├── rbc/                    # C++ source code
│   ├── core/              # rbc_core.dll - Core data structures
│   ├── runtime/           # rbc_runtime.dll - Runtime features
│   ├── editor/            # Editor application
│   ├── ext_c/             # Python binding (pybind11)
│   ├── render_plugin/     # Rendering pipeline
│   ├── tests/             # C++ tests
│   └── shader/            # Shader code
├── src/                    # Python source
│   ├── rbc_meta/          # Code generation metadata
│   ├── rbc_ext/           # C++ binding wrappers
│   └── robocute/          # Core Python package
├── custom_nodes/           # Custom node extensions
├── docs/                   # Documentation
├── samples/                # Python samples
├── test/                   # Python tests
├── thirdparty/            # C++ third-party libraries
└── xmake/                 # Xmake build scripts
```

## Development Environment Setup

### Prerequisites

1. **Network**: Access to GitHub
2. **Xmake**: C++ build system (v3.0.6+)
3. **LLVM/Clang**: Compiler toolchain (clang-cl on Windows)
4. **uv**: Python package manager
5. **Qt 6.8+**: For GUI editor
6. **7-zip**: For extracting prebuilt resources

### Initial Setup

```bash
# 1. Clone and enter the repository
git clone <repo-url>
cd RoboCute

# 2. Sync Python environment with dev extra
uv sync --extra dev          # Development tools

# 3. Prepare Project dependencies
uv run prepare -y

# 4. Generate code from metadata
uv run gen
```

## Build Workflows

### Standard C++ Build (Xmake)

```bash
# Configure (first time only)
xmake f -m debug -c
# commonly used platform
xmake f -p windows -m debug --toolchain=clang-cl -c
# Or release mode:
xmake f -m release -c

# Build all targets
xmake

# Build specific target
xmake <target_name>

# Build with verbose output
xmake -v

# List all targets
xmake -l
```

### Python Extension Build

```bash
# Build and install Python extension (release, no stub)
uv run pre-pack

# Debug mode with stub generation
uv run pre-pack debug uv

# Release mode with system stubgen
uv run pre-pack release
```

## Running the Application

### Editor Workflow

```bash
# Terminal 1: Start Python development server
uv run main.py

# Terminal 2: Run C++ editor
xmake run rbc-editor
```

### Graphics Tests

```bash
# Run graphics test with backend
xmake run test_graphics_bin <backend> <asset_dir> <intermediate_dir>

# Example:
xmake run test_graphics_bin dx ./assets ./.rbc
```

## Testing

### Python Tests

```bash
# Run all Python tests
uv run pytest

# Run specific test file
uv run pytest test/test_specific.py

# Run with verbose output
uv run pytest -v
```

### C++ Tests

```bash
# Run specific test target
xmake run <test_target>

# Available test targets (examples)
xmake run test_core
xmake run test_graphics
xmake run test_world
```

## Code Generation

RoboCute uses extensive code generation for serialization and Python bindings.

```bash
# Generate all code from metadata
uv run gen

# This generates:
# - Serialization code for C++ classes
# - Python binding code (pybind11)
# - Interface definitions
```

**Important**: Always run `uv run gen` after modifying:
- Files in `src/rbc_meta/`
- Class definitions with reflection macros
- Serialization-related code

## IDE Support

### Generate Compile Commands (for clangd)

```bash
# Xmake
xmake project -k compile_commands

# This creates compile_commands.json for clangd/LSP
```

### Visual Studio

```bash
# Generate VS2022 solution
xmake project -k vsxmake2022

# Solution will be in vsxmake2022/ folder
```

### QtCreator

1. Open root `CMakeLists.txt` directly
2. Configure with your kit
3. Build normally

## Common Tasks

### Clean Build

```bash
# Clean build artifacts
xmake clean

# Clean everything including config
xmake clean -a

# Then reconfigure
xmake f -m debug -c
```

### Update Dependencies

```bash
# Update Python dependencies
uv sync

# Update C++ dependencies (re-download thirdparty)
uv run prepare -y
```

### Resource Management

```bash
# Install resources after build
uv run install

# This copies shaders, default scenes, and render resources
```

## Debugging

### Debug Build

```bash
# Configure for debug
xmake f -m debug -c

# Build
xmake

# Run with debugger
xmake run -d <target>
```

### Address Sanitizer

See `docs/dev/profile/use_asan.md` for ASan configuration.

### Logging

- Logs are stored in `.rbc/logs/`
- SQLite database: `.rbc/logs/log.db`
- Text logs: `.rbc/logs/app_YYYY_MM_DD.log`

## Documentation

### Local Preview

```bash
# Serve docs locally
mkdocs serve

# Open http://127.0.0.1:8000
```

### Deploy Documentation

```bash
# Manual deployment
mkdocs gh-deploy

# Or push to main branch (auto-deploy via GitHub Actions)
```

### Documentation Structure

```
docs/
├── BUILD.md              # Build instructions
├── design/               # Architecture & design docs
│   ├── Architecture.md
│   ├── Pipeline.md
│   └── editor/           # Editor design
├── dev/                  # Developer guides
│   ├── Codebase.md
│   ├── EditorDev.md
│   └── codebase/         # Infrastructure docs
├── devlog/               # Development logs
├── tutorials/            # User tutorials
└── user-guide/           # User documentation
```

## Troubleshooting

### Build Issues

**Problem**: `xmake` fails with missing dependencies
```bash
# Solution: Re-run prepare
uv run prepare -y
```

**Problem**: Code generation errors
```bash
# Solution: Regenerate code
uv run gen
```

**Problem**: Python extension not found
```bash
# Solution: Rebuild and install
uv run pre-pack
```

### Runtime Issues

**Problem**: Editor crashes on startup
- Check Qt6 is in PATH
- Verify resources are installed: `uv run install`
- Check logs in `.rbc/logs/`

**Problem**: Graphics tests fail
- Verify GPU drivers
- Check backend availability (dx/vk/cuda)
- Ensure assets directory exists

### IDE Issues

**Problem**: IntelliSense not working
```bash
# Regenerate compile commands
xmake project -k compile_commands
```

**Problem**: QtCreator can't find Qt
- Set Qt6_DIR environment variable
- Or specify in CMake configuration

## Git Workflow

### Branch Structure

- `master`: Stable release branch
- `dev`: Main development branch
- `anim`: Animation development
- `doc`: Documentation updates
- `gh-pages`: Documentation site

### Commit Guidelines

1. Feature branches should target `dev`
2. Small, focused commits
3. Update docs for user-facing changes
4. Test before merging

## Best Practices

### Before Starting Work

1. Pull latest changes: `git pull`
2. Sync Python env: `uv sync`
3. Run code generation: `uv run gen`
4. Verify build: `xmake`

### During Development

1. Make incremental changes
2. Test frequently: `uv run pytest` or `xmake run <test>`
3. Use code generation for new serializable classes
4. Follow naming conventions (see cpp-master skill)

### Before Committing

1. Run tests: `uv run pytest`
2. Verify C++ build: `xmake`
3. Update documentation if needed
4. Check for debug prints or temporary files

### Release Preparation

1. Update version numbers
2. Run full test suite
3. Build release binaries: `xmake f -m release -c && xmake`
4. Update changelog
5. Tag release

## Quick Reference

```bash
# Full clean build workflow
xmake clean -a
uv sync
uv run prepare -y
uv run gen
xmake f -m release -c
xmake
uv run pre-pack

# Quick test cycle
xmake && xmake run <target>

# Python development
uv run main.py          # Start server
uv run pytest           # Run tests

# Editor development
uv run main.py          # Terminal 1
xmake run rbc-editor    # Terminal 2
```
