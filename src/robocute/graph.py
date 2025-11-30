"""
节点图管理

提供节点图的构建、验证和序列化功能。
"""
from typing import Dict, List, Any, Optional, Tuple, TYPE_CHECKING
from pydantic import BaseModel, Field
from .node_base import RBCNode
from .node_registry import get_registry

if TYPE_CHECKING:
    from .scene_context import SceneContext


class NodeConnection(BaseModel):
    """节点连接定义"""
    from_node: str  # 源节点ID
    from_output: str  # 源节点输出名称
    to_node: str  # 目标节点ID
    to_input: str  # 目标节点输入名称


class NodeDefinition(BaseModel):
    """节点定义"""
    node_id: str
    node_type: str
    inputs: Dict[str, Any] = Field(default_factory=dict)  # 静态输入值
    position: Optional[Dict[str, float]] = None  # UI位置（可选）


class GraphDefinition(BaseModel):
    """图定义"""
    nodes: List[NodeDefinition]
    connections: List[NodeConnection]
    metadata: Dict[str, Any] = Field(default_factory=dict)


class NodeGraph:
    """
    节点图
    
    管理节点实例和它们之间的连接关系。
    """
    
    def __init__(self, graph_id: str = "default", scene_context: Optional['SceneContext'] = None):
        """
        初始化节点图
        
        Args:
            graph_id: 图的唯一标识符
            scene_context: Optional scene context for nodes to access scene data
        """
        self.graph_id = graph_id
        self.scene_context = scene_context
        self._nodes: Dict[str, RBCNode] = {}
        self._connections: List[NodeConnection] = []
        self._metadata: Dict[str, Any] = {}
    
    def add_node(self, node_id: str, node_type: str, inputs: Optional[Dict[str, Any]] = None) -> Optional[RBCNode]:
        """
        添加节点到图中
        
        Args:
            node_id: 节点ID
            node_type: 节点类型
            inputs: 静态输入值（可选）
            
        Returns:
            创建的节点实例，如果节点类型不存在则返回 None
        """
        if node_id in self._nodes:
            raise ValueError(f"Node with id '{node_id}' already exists in the graph")
        
        registry = get_registry()
        node = registry.create_node(node_type, node_id, self.scene_context)
        
        if node is None:
            return None
        
        # 设置静态输入值
        if inputs:
            for name, value in inputs.items():
                node.set_input(name, value)
        
        self._nodes[node_id] = node
        return node
    
    def remove_node(self, node_id: str) -> bool:
        """
        从图中移除节点
        
        Args:
            node_id: 节点ID
            
        Returns:
            是否成功移除
        """
        if node_id not in self._nodes:
            return False
        
        # 移除相关连接
        self._connections = [
            conn for conn in self._connections
            if conn.from_node != node_id and conn.to_node != node_id
        ]
        
        del self._nodes[node_id]
        return True
    
    def get_node(self, node_id: str) -> Optional[RBCNode]:
        """
        获取节点实例
        
        Args:
            node_id: 节点ID
            
        Returns:
            节点实例，如果不存在则返回 None
        """
        return self._nodes.get(node_id)
    
    def add_connection(self, from_node: str, from_output: str, to_node: str, to_input: str) -> bool:
        """
        添加节点连接
        
        Args:
            from_node: 源节点ID
            from_output: 源节点输出名称
            to_node: 目标节点ID
            to_input: 目标节点输入名称
            
        Returns:
            是否成功添加连接
        """
        # 验证节点存在
        if from_node not in self._nodes or to_node not in self._nodes:
            return False
        
        # 创建连接
        connection = NodeConnection(
            from_node=from_node,
            from_output=from_output,
            to_node=to_node,
            to_input=to_input
        )
        
        self._connections.append(connection)
        return True
    
    def get_connections(self) -> List[NodeConnection]:
        """
        获取所有连接
        
        Returns:
            连接列表
        """
        return self._connections.copy()
    
    def get_node_connections(self, node_id: str) -> Tuple[List[NodeConnection], List[NodeConnection]]:
        """
        获取节点的输入和输出连接
        
        Args:
            node_id: 节点ID
            
        Returns:
            (输入连接列表, 输出连接列表)
        """
        input_connections = [conn for conn in self._connections if conn.to_node == node_id]
        output_connections = [conn for conn in self._connections if conn.from_node == node_id]
        return input_connections, output_connections
    
    def topological_sort(self) -> Optional[List[str]]:
        """
        对图进行拓扑排序
        
        Returns:
            排序后的节点ID列表，如果存在循环则返回 None
        """
        # 计算每个节点的入度
        in_degree: Dict[str, int] = {node_id: 0 for node_id in self._nodes}
        
        for conn in self._connections:
            in_degree[conn.to_node] += 1
        
        # 找到所有入度为0的节点
        queue: List[str] = [node_id for node_id, degree in in_degree.items() if degree == 0]
        result: List[str] = []
        
        while queue:
            # 取出一个入度为0的节点
            current = queue.pop(0)
            result.append(current)
            
            # 减少所有后继节点的入度
            for conn in self._connections:
                if conn.from_node == current:
                    in_degree[conn.to_node] -= 1
                    if in_degree[conn.to_node] == 0:
                        queue.append(conn.to_node)
        
        # 如果所有节点都被访问，则没有循环
        if len(result) == len(self._nodes):
            return result
        else:
            return None  # 存在循环
    
    def validate(self) -> Tuple[bool, Optional[str]]:
        """
        验证图的有效性
        
        Returns:
            (是否有效, 错误消息)
        """
        # 检查是否有节点
        if not self._nodes:
            return False, "Graph has no nodes"
        
        # 检查连接的节点是否存在
        for conn in self._connections:
            if conn.from_node not in self._nodes:
                return False, f"Connection references non-existent node: {conn.from_node}"
            if conn.to_node not in self._nodes:
                return False, f"Connection references non-existent node: {conn.to_node}"
        
        # 检查是否存在循环
        if self.topological_sort() is None:
            return False, "Graph contains cycles"
        
        return True, None
    
    def to_dict(self) -> Dict[str, Any]:
        """
        将图转换为字典
        
        Returns:
            图的字典表示
        """
        nodes = []
        for node_id, node in self._nodes.items():
            # 获取节点类型
            node_type = node.NODE_TYPE
            # 获取静态输入
            inputs = node._inputs.copy()
            
            nodes.append({
                "node_id": node_id,
                "node_type": node_type,
                "inputs": inputs
            })
        
        connections = [conn.model_dump() for conn in self._connections]
        
        return {
            "graph_id": self.graph_id,
            "nodes": nodes,
            "connections": connections,
            "metadata": self._metadata
        }
    
    @classmethod
    def from_definition(cls, definition: GraphDefinition, graph_id: str = "default", scene_context: Optional['SceneContext'] = None) -> 'NodeGraph':
        """
        从图定义创建节点图
        
        Args:
            definition: 图定义
            graph_id: 图ID
            scene_context: Optional scene context for nodes
            
        Returns:
            节点图实例
            
        Raises:
            ValueError: 如果定义无效
        """
        graph = cls(graph_id, scene_context)
        
        # 添加节点
        for node_def in definition.nodes:
            node = graph.add_node(node_def.node_id, node_def.node_type, node_def.inputs)
            if node is None:
                raise ValueError(f"Failed to create node: {node_def.node_id} (type: {node_def.node_type})")
        
        # 添加连接
        for conn in definition.connections:
            success = graph.add_connection(
                conn.from_node, conn.from_output,
                conn.to_node, conn.to_input
            )
            if not success:
                raise ValueError(f"Failed to create connection: {conn}")
        
        # 设置元数据
        graph._metadata = definition.metadata
        
        return graph
    
    def clear(self) -> None:
        """清空图"""
        self._nodes.clear()
        self._connections.clear()
        self._metadata.clear()
    
    def __len__(self) -> int:
        """返回节点数量"""
        return len(self._nodes)
    
    def __repr__(self) -> str:
        return f"<NodeGraph(id={self.graph_id}, nodes={len(self._nodes)}, connections={len(self._connections)})>"

