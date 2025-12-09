"""
节点基类定义

提供 RBCNode 基类，用于定义节点的基本结构和行为。
"""

from abc import ABC, abstractmethod
from typing import Any, Dict, List, Optional
from pydantic import BaseModel, Field

from .context import SceneContext


class NodeInput(BaseModel):
    """节点输入定义"""

    name: str
    type: str
    required: bool = True
    default: Optional[Any] = None
    description: str = ""


class NodeOutput(BaseModel):
    """节点输出定义"""

    name: str
    type: str
    description: str = ""


class NodeMetadata(BaseModel):
    """节点元数据"""

    node_type: str
    display_name: str
    category: str = "default"
    description: str = ""
    inputs: List[NodeInput] = Field(default_factory=list)
    outputs: List[NodeOutput] = Field(default_factory=list)


class RBCNode(ABC):
    """
    节点基类

    所有自定义节点都应继承此类并实现 execute 方法。
    """

    # 子类需要定义这些类属性
    NODE_TYPE: str = "base_node"
    DISPLAY_NAME: str = "Base Node"
    CATEGORY: str = "default"
    DESCRIPTION: str = ""

    def __init__(self, node_id: str, context: Optional[SceneContext] = None):
        """
        初始化节点

        Args:
            node_id: 节点的唯一标识符
            context: Optional scene context for accessing scene data
        """
        self.node_id = node_id
        self.context = context
        self._inputs: Dict[str, Any] = {}
        self._outputs: Dict[str, Any] = {}

    @classmethod
    @abstractmethod
    def get_inputs(cls) -> List[NodeInput]:
        """
        获取节点输入定义

        Returns:
            输入定义列表
        """
        pass

    @classmethod
    @abstractmethod
    def get_outputs(cls) -> List[NodeOutput]:
        """
        获取节点输出定义

        Returns:
            输出定义列表
        """
        pass

    @classmethod
    def get_metadata(cls) -> NodeMetadata:
        """
        获取节点元数据

        Returns:
            节点元数据
        """
        return NodeMetadata(
            node_type=cls.NODE_TYPE,
            display_name=cls.DISPLAY_NAME,
            category=cls.CATEGORY,
            description=cls.DESCRIPTION,
            inputs=cls.get_inputs(),
            outputs=cls.get_outputs(),
        )

    def set_input(self, name: str, value: Any) -> None:
        """
        设置输入值

        Args:
            name: 输入名称
            value: 输入值
        """
        self._inputs[name] = value

    def get_input(self, name: str, default: Any = None) -> Any:
        """
        获取输入值

        Args:
            name: 输入名称
            default: 默认值

        Returns:
            输入值
        """
        return self._inputs.get(name, default)

    def set_output(self, name: str, value: Any) -> None:
        """
        设置输出值

        Args:
            name: 输出名称
            value: 输出值
        """
        self._outputs[name] = value

    def get_output(self, name: str) -> Any:
        """
        获取输出值

        Args:
            name: 输出名称

        Returns:
            输出值
        """
        return self._outputs.get(name)

    def get_all_outputs(self) -> Dict[str, Any]:
        """
        获取所有输出值

        Returns:
            所有输出值的字典
        """
        return self._outputs.copy()

    @abstractmethod
    def execute(self) -> Dict[str, Any]:
        """
        执行节点逻辑

        子类必须实现此方法来定义节点的具体行为。

        Returns:
            输出结果字典，键为输出名称，值为输出值
        """
        pass

    def run(self) -> Dict[str, Any]:
        """
        运行节点

        执行节点并保存输出结果。

        Returns:
            输出结果字典
        """
        # 执行节点逻辑
        outputs = self.execute()

        # 保存输出
        for name, value in outputs.items():
            self.set_output(name, value)

        return outputs

    def reset(self) -> None:
        """重置节点状态"""
        self._inputs.clear()
        self._outputs.clear()

    def __repr__(self) -> str:
        return f"<{self.__class__.__name__}(id={self.node_id})>"
