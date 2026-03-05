from rbc_meta.utils.reflect import reflect
from typing import List

@reflect(
    cpp_namespace="rbc",
    module_name="rbc_project_plugin"
)
class ProjectSettingPaths:
    assets: str
    intermediate: str 
    
@reflect(
    cpp_namespace="rbc",
    module_name="rbc_project_plugin"
)
class ProjectDefaultConfig:
    default_scene: str
    startup_graph: str 
    graphics_backend: str 
    resource_version: str

@reflect(
    cpp_namespace="rbc",
    module_name="rbc_project_plugin"
)
class ProjectMetaData:
    tags: List[str]
    license: str
    repository: str 

@reflect(
    cpp_namespace="rbc",
    module_name="rbc_project_plugin"
)
class ProjectSetting:
    name: str
    version: str
    rbc_version: str
    author: str
    description: str
    paths: ProjectSettingPaths
    config: ProjectDefaultConfig

OUT_CLASSES = [
    ProjectDefaultConfig,
    ProjectSettingPaths,
    ProjectSetting
]

__all__ = [
    "OUT_CLASSES"
]