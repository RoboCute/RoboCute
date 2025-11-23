# RoboCute

- rbc_editor: The Editor via Qt and LuisaCompute
- rbc_ext: wrap the editor 
- doc: documentatino

## How to Get Started

- see [[BUILD.md]]


## Architecture

- src
  - rbc_meta: python和cpp的通用codegen入口，元信息结构定义
  - rbc_ext: cpp pybind的封装层
  - robocute: python的核心功能
