from rbc_meta.utils.reflect import reflect
from typing import List, Optional

@reflect(
    cpp_namespace="rbc"
)
class ProjectSettingPaths:
    assets: Optional[str]
    intermediate: Optional[str] 
    
@reflect(
    cpp_namespace="rbc"
)
class ProjectDefaultConfig:
    default_scene: Optional[str]
    startup_graph: Optional[str] 
    graphics_backend: Optional[str] 
    resource_version: Optional[str]

@reflect(
    cpp_namespace="rbc"
)
class ProjectMetaData:
    tags: List[str]
    license: Optional[str]
    repository: Optional[str] 

@reflect(
    cpp_namespace="rbc"
)
class ProjectSetting:
    name: str
    version: str
    rbc_version: str
    author: Optional[str]
    description: Optional[str]
    paths: Optional[ProjectSettingPaths]
    config: Optional[ProjectDefaultConfig]

OUT_CLASSES = [
    ProjectDefaultConfig,
    ProjectSettingPaths,
    ProjectSetting
]

__all__ = [
    "OUT_CLASSES"
]