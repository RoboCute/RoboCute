# RoboCute

- rbc_editor: The Editor via Qt and LuisaCompute
- rbc_ext: wrap the editor 
- doc: documentatino

## How to Get Started

- see [[BUILD.md]]


## Architecture

- rbc: C++ Project
  - core: rbc_core.dll
  - world: rbc_world.dll for Scena and Resource
  - runtime: rbc_runtime.dll
  - ext_c: rbc_ext_c.pyd Core Python Binding DLL
  - render_plugin
  - samples: C++ Samples
  - tests: C++ Tests
  - shader
- src
  - rbc_meta: python和cpp的通用codegen入口，元信息结构定义
  - rbc_ext: cpp pybind的封装层
  - robocute: python的核心功能


## How to Play

- `uv run main.py` start the python server
- `xmake run rbc_editor` start the editor
- find the registered node
- connect and execute
- fetch the result from python side
- playback in editor