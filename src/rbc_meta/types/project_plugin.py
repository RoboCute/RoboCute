from rbc_meta.utils.reflect import reflect
from enum import Enum

@reflect(
    pybind=True,
    module_name="rbc_project_plugin"
)
class ProjectSetting(Enum):
    name: str
    version: str
    rbc_version: str
    author: str
    description: str


OUT_CLASSES = [
    ProjectSetting
]

__all__ = [
    "OUT_CLASSES"
]