# Agent Guidelines

## Rules

- never change files under `generated/`, `thirdparty/`.
- use `uv sync --extra=all` to sync package
- use `uv run` to run python scripts

## C++ Lint

Run C++ syntax validation, or get file reference using clangd:
```bash
uv run scripts/cpp_lint.py <cpp_file> [--project-root <dir>] [--clangd-path <path>] [-v]
```
after write c++ file, use `build` skill to build.

## Python Lint
Run Python syntax check (and optionally execute):
```bash
uv run scripts/py_lint.py <py_file> [--exec]
```

## Build & Configuration
Use the `build` skill to configure and build the project. Typical workflow:

```bash
# Prepare environment (downloads tools/resources)
uv run prepare -y

# Configure xmake build
xmake f -m releasedbg -c

# Build all targets
xmake

# Or build and install Python extension artifacts
uv run scripts/build_and_copy.py releasedbg uv
```

Refer to the `build` skill (`.agent/skills/build/SKILL.md`) for full details on xmake options, packaging/unpacking toolchain archives, and Python-to-C++ codegen.
