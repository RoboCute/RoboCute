import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen_util as ut
from rbc_meta.utils.codegen import cpp_interface_gen, cpp_impl_gen
from pathlib import Path


def codegen_header(header_path: Path, cpp_path: Path):
    rbcResourceType = tr.enum(
        "rbc::ResourceType",
        Unknown=1,
        Mesh=1,
        Texture=2,
        Material=3,
        Shader=4,
        Animation=5,
        Skeleton=6,
        PhysicsShape=7,
        Audio=8,
        Custom=1000,
    )

    rbcResourceState = tr.enum(
        "rbc::ResourceState",
        Unloaded=1,  # 未加载
        Pending=2,  # 加载请求已提交
        Loading=3,  # 正在加载
        Loaded=4,  # 加载完成（serde类结构加载从本地磁盘/内存/网络加载完成）
        Installed=5,  # 安装完成（上传GPU，依赖处理等完成，可以被其他组件直接使用）
        Failed=6,  # 加载失败
        Unloading=7,  # 正在卸载
    )

    ut.codegen_to(header_path)(cpp_interface_gen)
