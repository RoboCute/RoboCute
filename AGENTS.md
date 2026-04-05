# Agent Guidelines

## Skill Selection

| Scenario | Skill |
|----------|-------|
| Complex C++ tasks | `cpp-master` |
| C++ only (default) | `cpp-slave` |
| Files in `rbc/runtime/include/rbc_world/` or `rbc/runtime/src/world/` | `world_resource` |
| Files in `rbc/importer_plugin/` | `importer` |
| Project maintenance | `maintain-project` |
| Python codegen interface (`src/rbc_meta/types/*.py`) | `py_codegen` |
| RoboCute Python API (entities, components, resources) | `py` |
| Python shader (load/dispatch) | `py_shader` |
| Python mesh construction | `mesh_builder` |
| UV config/build | `uv` |
| Xmake build | `xmake` |

## Quick Decision

```
Python/UV task? → uv
Xmake build? → xmake
Python shader load/dispatch? → py_shader
Python mesh construction? → mesh_builder
Python codegen interface? → py_codegen
RoboCute Python API? → py
C++ in rbc_world/? → world_resource
C++ in rbc/importer_plugin/? → importer
C++ only? → cpp-slave
```
