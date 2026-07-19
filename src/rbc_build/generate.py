"""
代码生成脚本 - 使用新 API
"""

from rbc_meta.utils.codegenx import CodegenRegistry, codegen, CodeModule


# 基础模块 - 只提供头文件依赖
@codegen
class LuisaResourceModule(CodeModule):
    name = "luisa_resource_module"
    header_files = [
        "luisa/runtime/image.h",
        "luisa/runtime/buffer.h",
        "luisa/runtime/rhi/pixel.h",
    ]


@codegen
class RBCCoreModule(CodeModule):
    name = "rbc_core_module"
    header_files = ["rbc_core/utils/curve.h"]


# 从类型定义导入类
from rbc_meta.types.resource import OUT_CLASSES as OUT_RESOURCE_CLASSES


@codegen
class ResourceMetaModule(CodeModule):
    name = "resource_meta_module"
    cpp_interface_header = "rbc/runtime/include/rbc_plugin/generated/resource_meta.hpp"
    cpp_impl_file = "rbc/runtime/src/generated/resource_meta.cpp"
    deps = [LuisaResourceModule]
    classes = OUT_RESOURCE_CLASSES


from rbc_meta.types.pipeline_settings import OUT_CLASSES as PIPELINE_SETTING_CLASSES
from rbc_meta.types.pipeline_settings import AlphaCull


@codegen
class PipelineSettingModule(CodeModule):
    name = "pipeline_setting_module"
    cpp_interface_header = (
        "rbc/render_plugin/include/rbc_render/generated/pipeline_settings.hpp"
    )
    cpp_impl_file = "rbc/render_plugin/src/generated/pipeline_settings.cpp"
    header_files = ["rbc_render/procedural/sky_atmosphere.h"]
    deps = [LuisaResourceModule, RBCCoreModule]
    classes = PIPELINE_SETTING_CLASSES


from rbc_meta.types.world_interface import OUT_CLASSES as OUT_WORLD_INTERFACE_CLASSES


@codegen
class WorldInterfaceModule(CodeModule):
    name = "world_interface"
    cpp_interface_header = "rbc/tests/test_graphics/include/generated/world.h"
    header_files = [
        "res_creation_info.h",
        "rbc_plugin/generated/resource_meta.hpp",
        "rbc_world/resources/mesh.h",
        "rbc_render/generated/pipeline_settings.hpp",
    ]
    classes = OUT_WORLD_INTERFACE_CLASSES
    deps = [ResourceMetaModule]


# from rbc_meta.types.editor_interface import OUT_CLASSES as OUT_EDITOR_INTERFACE_CLASSES


# @codegen
# class EditorInterfaceModule(CodeModule):
#     name = "editor_interface"
#     cpp_interface_header = "rbc/editor/runtime/include/generated/editor_interface.hpp"
#     header_files = [
#         "RBCEditorRuntime/infra/editor/EditorProject.h",
#         "RBCEditorRuntime/infra/editor/EditorScene.h",
#     ]
#     classes = OUT_EDITOR_INTERFACE_CLASSES


from rbc_meta.types.project_plugin import OUT_CLASSES as OUT_PROJECT_PLUGIN_CLASSES
from rbc_meta.types.project_plugin import (
    SCHEMA_VERSION as PROJECT_SCHEMA_VERSION,
    CURRENT_SCHEMA_VERSION as PROJECT_CURRENT_SCHEMA_VERSION,
)


@codegen
class ProjectPluginModule(CodeModule):
    name = "project_plugin"
    cpp_interface_header = "rbc/project_plugin/include/rbc_project/generated/project.h"
    cpp_impl_file = "rbc/project_plugin/src/generated/project.cpp"
    py_schema_file = "src/robocute/generated/project_schema.py"
    py_schema_root = "ProjectConfigSchema"
    extra_constants = {
        "RBC_PROJECT_SCHEMA_VERSION": PROJECT_SCHEMA_VERSION,
        "RBC_PROJECT_CURRENT_SCHEMA_VERSION": PROJECT_CURRENT_SCHEMA_VERSION,
    }
    classes = OUT_PROJECT_PLUGIN_CLASSES


# Python 绑定模块
EXT_CLASSES = []
EXT_CLASSES.extend(OUT_WORLD_INTERFACE_CLASSES)
EXT_CLASSES.extend(OUT_RESOURCE_CLASSES)
EXT_CLASSES.append(AlphaCull)
# EXT_CLASSES.extend(OUT_EDITOR_INTERFACE_CLASSES)


@codegen
class WorldInterfacePybindModule(CodeModule):
    name = "rbc_ext_c"
    header_files = [
        "generated/world.h",
    ]
    pybind_cpp_def_file = "rbc/extensions/ext_c/src/generated/world.cpp"
    pybind_py_file = "src/robocute/rbc_ext/generated/world.py"
    classes = EXT_CLASSES
    deps = [WorldInterfaceModule]


def generate_registered():
    r = CodegenRegistry()
    r.generate()


if __name__ == "__main__":
    generate_registered()
