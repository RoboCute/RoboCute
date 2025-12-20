from rbc_meta.utils.reflect import reflect
from enum import Enum
from rbc_meta.utils.builtin import IRTTRBasic, Pointer, RenderMesh


@reflect(cpp_namespace="rbc", module_name="resource_type", pybind=True)
class ResourceType(Enum):
    Unknown = 0
    Mesh = 1
    Texture = 2
    Material = 3
    Shader = 4
    Animation = 5
    Skeleton = 6
    PhysicsShape = 7
    Audio = 8
    Custom = 1000


@reflect(cpp_namespace="rbc", module_name="resource_type", pybind=True)
class ResourceState(Enum):
    Unloaded = 1  # 未加载
    Pending = 2  # 加载请求已提交
    Loading = 3  # 正在加载
    Loaded = 4  # 加载完成（serde类结构加载从本地磁盘/内存/网络加载完成）
    Installed = 5  # 安装完成（上传GPU，依赖处理等完成，可以被其他组件直接使用）
    Failed = 6  # 加载失败
    Unloading = 7  # 正在卸载


@reflect(cpp_namespace="rbc", module_name="resource_type")
class IResource(IRTTRBasic):
    pass


@reflect(cpp_namespace="rbc", module_name="resource_type")
class MeshResource(IResource):
    render_mesh: Pointer[RenderMesh]
