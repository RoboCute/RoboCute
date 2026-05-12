# Agent Guidelines

## Rules
- never change files under `generated/`.
- use `uv sync --extra=all` to sync package
- use `uv run` to run python scripts

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
