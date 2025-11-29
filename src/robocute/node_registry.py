"""
节点注册系统

提供节点注册中心和装饰器，用于管理所有可用的节点类型。
"""

from typing import Dict, Type, List, Optional
from .node_base import RBCNode, NodeMetadata


class NodeRegistry:
    """
    节点注册中心（单例模式）

    管理所有注册的节点类型，提供节点查询和实例化功能。
    """

    _instance: Optional["NodeRegistry"] = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._registry: Dict[str, Type[RBCNode]] = {}
        return cls._instance

    def register(self, node_class: Type[RBCNode]) -> Type[RBCNode]:
        """
        注册节点类

        Args:
            node_class: 节点类

        Returns:
            节点类（用于装饰器）

        Raises:
            ValueError: 如果节点类型已存在
        """
        node_type = node_class.NODE_TYPE

        if node_type in self._registry:
            raise ValueError(f"Node type '{node_type}' is already registered")

        # 验证节点类是否正确继承
        if not issubclass(node_class, RBCNode):
            raise TypeError(f"{node_class.__name__} must inherit from RBCNode")

        self._registry[node_type] = node_class
        print(f"Registered node: {node_type} ({node_class.DISPLAY_NAME})")

        return node_class

    def unregister(self, node_type: str) -> bool:
        """
        取消注册节点类型

        Args:
            node_type: 节点类型

        Returns:
            是否成功取消注册
        """
        if node_type in self._registry:
            del self._registry[node_type]
            return True
        return False

    def get_node_class(self, node_type: str) -> Optional[Type[RBCNode]]:
        """
        根据节点类型获取节点类

        Args:
            node_type: 节点类型

        Returns:
            节点类，如果不存在则返回 None
        """
        return self._registry.get(node_type)

    def create_node(self, node_type: str, node_id: str) -> Optional[RBCNode]:
        """
        创建节点实例

        Args:
            node_type: 节点类型
            node_id: 节点ID

        Returns:
            节点实例，如果节点类型不存在则返回 None
        """
        node_class = self.get_node_class(node_type)
        if node_class is None:
            return None

        return node_class(node_id)

    def get_all_node_types(self) -> List[str]:
        """
        获取所有已注册的节点类型

        Returns:
            节点类型列表
        """
        return list(self._registry.keys())

    def get_all_metadata(self) -> List[NodeMetadata]:
        """
        获取所有节点的元数据

        Returns:
            节点元数据列表
        """
        return [node_class.get_metadata() for node_class in self._registry.values()]

    def get_metadata(self, node_type: str) -> Optional[NodeMetadata]:
        """
        获取指定节点类型的元数据

        Args:
            node_type: 节点类型

        Returns:
            节点元数据，如果不存在则返回 None
        """
        node_class = self.get_node_class(node_type)
        if node_class is None:
            return None
        return node_class.get_metadata()

    def clear(self) -> None:
        """清空注册表"""
        self._registry.clear()

    def __len__(self) -> int:
        """返回已注册节点数量"""
        return len(self._registry)

    def __contains__(self, node_type: str) -> bool:
        """检查节点类型是否已注册"""
        return node_type in self._registry


# 全局注册中心实例
_global_registry = NodeRegistry()


def register_node(node_class: Type[RBCNode] = None):
    """
    节点注册装饰器

    用法：
        @register_node
        class MyNode(RBCNode):
            ...

    Args:
        node_class: 节点类

    Returns:
        装饰后的节点类
    """

    def decorator(cls: Type[RBCNode]) -> Type[RBCNode]:
        return _global_registry.register(cls)

    if node_class is None:
        # 作为 @register_node() 使用
        return decorator
    else:
        # 作为 @register_node 使用
        return decorator(node_class)


def get_registry() -> NodeRegistry:
    """
    获取全局注册中心实例

    Returns:
        全局注册中心
    """
    return _global_registry
