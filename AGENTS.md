# Agent Guidelines

## Rules
1. never change files under `generated/`.
2. 

## C++ Lint
Run C++ syntax validation, or get file reference using clangd:
```bash
uv run scripts/cpp_lint.py <cpp_file> [--project-root <dir>] [--clangd-path <path>] [-v]
```
after write c++ file, use `xmake` skill to build.

## Python Lint
Run Python syntax check (and optionally execute):
```bash
uv run scripts/py_lint.py <py_file> [--exec]
```
