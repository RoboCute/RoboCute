# Agent Guidelines

## Lint Scripts

### C++ Lint
Run C++ syntax validation using clangd:
```bash
uv run scripts/cpp_lint.py <cpp_file> [--project-root <dir>] [--clangd-path <path>] [-v]
```

### Python Lint
Run Python syntax check (and optionally execute):
```bash
uv run scripts/py_lint.py <py_file> [--exec]
```
