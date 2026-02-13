# GenerateX

下一代代码生成脚本

## Runtime

- [x] Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\runtime\include\rbc_plugin\generated\resource_meta.hpp
- [x] Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\runtime\src\generated\resource_meta.cpp

## rbc_render

- [x] Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\render_plugin\include\rbc_render\generated\pipeline_settings.hpp
- [x] Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\render_plugin\src\generated\pipeline_settings.cpp


## world_interface

pybind_codegen
- get_enum_binding
- get_struct_binding

- [ ] Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_graphics\generated\world.h
- [ ] Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_py_codegen\generated\world.cpp
- [ ] Writing to D:\ws\repos\RoboCute-repo\RoboCute\src\rbc_ext\generated\world.py

World Interface的需求复杂且重要，需要仔细设计一下

enum需求

export_xxx
py::enum_<Enum>(m, "name").value("name", value)..
static ModuleRegister export_test_world_(export_test_world);

## Serde

DEPRECATED

- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_serde\generated\generated.hpp
- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_serde\generated\enum_ser.cpp
- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_serde\generated\client.hpp
- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_serde\generated\client.cpp

## IPC生成

DEPRECATED

- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_ipc\generated\server.hpp
- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_ipc\generated\server.cpp
- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_ipc\generated\client.hpp
- Writing to D:\ws\repos\RoboCute-repo\RoboCute\rbc\tests\test_ipc\generated\client.cpp