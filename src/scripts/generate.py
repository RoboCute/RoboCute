from rbc_meta.utils.codegenx import CodegenResitry, codegen, CodeModule

@codegen
class LuisaResourceModule(CodeModule):
    name_ = "luisa_resource_module"
    header_files_ = [
        "luisa/runtime/image.h",
        "luisa/runtime/buffer.h",
        "luisa/runtime/rhi/pixel.h",
    ]

@codegen
class RBCCoreModule(CodeModule):
    name_ = "rbc_core_module"
    header_files_ = ["rbc_core/utils/curve.h"]


from rbc_meta.types.resource import OUT_CLASSES as OUT_RESOURCE_CLASSES
@codegen
class ResourceMetaModule(CodeModule):
    name_ = "resource_meta_module"
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/runtime/"
    interface_header_file_ = "rbc_plugin/generated/resource_meta.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/resource_meta.cpp"
    deps_ = [LuisaResourceModule]
    classes_ = OUT_RESOURCE_CLASSES


from rbc_meta.types.pipeline_settings import OUT_CLASSES as PIPELINE_SETTING_CLASSES
@codegen
class PipelineSettingModule(CodeModule):
    enable_cpp_interface_ = True
    cpp_base_dir_ = "rbc/render_plugin/"
    interface_header_file_ = "rbc_render/generated/pipeline_settings.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "generated/pipeline_settings.cpp"
    header_files_ = ["rbc_render/procedural/sky_atmosphere.h"]
    deps_ = [LuisaResourceModule, RBCCoreModule]
    classes_ = PIPELINE_SETTING_CLASSES


from rbc_meta.types.world_interface import OUT_CLASSES as OUT_WORLD_INTERFACE_CLASSES
@codegen
class WorldInterfaceModule(CodeModule):
    name_ = "world_interface"
    cpp_base_dir_ = "rbc/tests/test_graphics"
    header_files_ = [
        "res_creation_info.h",
        "rbc_plugin/generated/resource_meta.hpp",
        "rbc_world/resources/mesh.h",
    ]
    enable_cpp_interface_ = True
    interface_header_file_ = "generated/world.h"
    classes_ = OUT_WORLD_INTERFACE_CLASSES
    deps_ = [ResourceMetaModule]


EXT_CLASSES = []
EXT_CLASSES.extend(OUT_WORLD_INTERFACE_CLASSES)
EXT_CLASSES.extend(OUT_RESOURCE_CLASSES)


@codegen
class WorldInterfacePybindModule(CodeModule):
    name_ = "test_py_codegen"
    header_files_ = ["generated/world.h"]
    enable_pybind_cpp_def_ = True
    pybind_cpp_def_file_ = "rbc/tests/test_py_codegen/generated/world.cpp"
    enable_pybind_ = True
    pybind_py_file_ = "src/robocute/rbc_ext/generated/world.py"
    classes_ = EXT_CLASSES
    deps_ = [WorldInterfaceModule]


@codegen
class WorldInterfacePybindModuleX(CodeModule):
    name_ = "rbc_ext_c"
    header_files_ = ["generated/world.h"]
    enable_pybind_cpp_def_ = True
    pybind_cpp_def_file_ = "rbc/extensions/ext_c/src/generated/world.cpp"
    enable_pybind_ = True
    pybind_py_file_ = "src/robocute/rbc_ext/generated/world_v2.py"
    classes_ = EXT_CLASSES
    deps_ = [WorldInterfaceModule]

from rbc_meta.types.project_plugin import OUT_CLASSES as OUT_PROJECT_PLUGIN_CLASSES

@codegen
class ProjectPluginMoudle(CodeModule):
    name_ = "project_plugin"
    enable_cpp_interface_ = True 
    cpp_base_dir_ = "rbc/project_plugin"
    interface_header_file_ = "rbc_project/generated/project.h"
    classes_ = OUT_PROJECT_PLUGIN_CLASSES

def generate_registered():
    r = CodegenResitry()
    r.generate()
