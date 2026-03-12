---
name: uv
---

# UV Package Manager

This project uses [uv](https://docs.astral.sh/uv/) as the Python package manager and virtual environment tool.

## Project Configuration

- **Python Version**: >= 3.13 (defined in `pyproject.toml`)
- **Virtual Environment**: `.venv/` (auto-created by uv)
- **Workspace**: Enabled with members in `packages/*`
- **Dependencies**: Listed in `pyproject.toml` [project.dependencies]

## Common Commands

### Environment Setup

```bash
# Create virtual environment and install dependencies
uv sync

# Install with dev dependencies
uv sync --extra dev

# Install with all extras
uv sync --all-extras
```

### Running Python Scripts

```bash
# Run a Python script (uses project environment)
uv run <script.py>

# Run a Python module
uv run -m <module_name>

# Run with arguments
uv run <script.py> -- <args>
```

### Dependency Management

```bash
# Add a dependency
uv add <package>

# Add a dev dependency
uv add --dev <package>

# Remove a dependency
uv remove <package>

# Update lock file
uv lock
```

### Build

```bash
# Build the project (creates distribution packages)
uv build
```

### Project Scripts (defined in pyproject.toml)

```bash
# Run project scripts
uv run prepare      # Prepare the project (git clone, download packages)
uv run gen          # Generate code
```

## Project Structure

- `pyproject.toml` - Project configuration and dependencies
- `uv.lock` - Lock file for reproducible builds
- `.venv/` - Virtual environment (auto-managed)
- `packages/` - Workspace members

## Integration with Build System

The project uses `uv_build` as the build backend. Python extensions are built via:

```bash
# Build and install C++ extensions
uv run scripts/build_and_copy.py debug 1
```

## Key Points

- Always use `uv run` instead of direct `python` to ensure correct environment
- The `.venv` directory is managed by uv; don't modify it manually
- Python path is detected from `.venv/pyvenv.cfg` during preparation
- The lock file (`uv.lock`) ensures reproducible builds across environments
