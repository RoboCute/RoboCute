# Agent Guidelines

## Skill Selection

| Scenario | Skill |
|----------|-------|
| C++ only (default) | `cpp-slave` |
| Files in `rbc/runtime/include/rbc_world/` or `rbc/runtime/src/world/` | `world_resource` |
| UV config/build | `uv` |
| Xmake build | `xmake` |

## Quick Decision

```
Python/UV task? → uv
Xmake build? → xmake
C++ in rbc_world/? → world_resource
C++ only? → cpp-slave
```
