# Agent Guidelines

## Rules

- never change files under `generated/`, `thirdparty/`.
- use `uv sync` to sync package
- use `uv run` to run python scripts

## Build & Configuration

Use the `build` skill to configure and build the project. Typical workflow:

```bash
# Prepare environment (downloads tools/resources)
uv run prepare -y

# Configure xmake build
xmake f -m releasedbg -c

# Build all targets
xmake

# Install the built packages into proper python path 
uv run pre-pack
```

Refer to the `build` skill (`.agent/skills/build/SKILL.md`) for full details on xmake options, packaging/unpacking toolchain archives, and Python-to-C++ codegen.
