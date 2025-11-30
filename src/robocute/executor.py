"""
图执行引擎

提供节点图的执行功能，按拓扑顺序执行节点。
"""
from typing import Dict, Any, List, Optional, Callable, TYPE_CHECKING
from enum import Enum
from datetime import datetime
from pydantic import BaseModel
from .graph import NodeGraph, NodeConnection

if TYPE_CHECKING:
    from .scene_context import SceneContext


class ExecutionStatus(str, Enum):
    """执行状态"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


class NodeExecutionResult(BaseModel):
    """节点执行结果"""
    node_id: str
    status: ExecutionStatus
    outputs: Dict[str, Any] = {}
    error: Optional[str] = None
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None
    duration_ms: Optional[float] = None


class GraphExecutionResult(BaseModel):
    """图执行结果"""
    graph_id: str
    status: ExecutionStatus
    node_results: Dict[str, NodeExecutionResult] = {}
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None
    duration_ms: Optional[float] = None
    error: Optional[str] = None


class GraphExecutor:
    """
    图执行器
    
    按拓扑顺序执行节点图中的所有节点。
    """
    
    def __init__(self, graph: NodeGraph, scene_context: Optional['SceneContext'] = None):
        """
        初始化执行器
        
        Args:
            graph: 要执行的节点图
            scene_context: Optional scene context for nodes to access scene data
        """
        self.graph = graph
        self.scene_context = scene_context
        self._execution_result: Optional[GraphExecutionResult] = None
        self._callbacks: List[Callable[[str, ExecutionStatus], None]] = []
    
    def add_callback(self, callback: Callable[[str, ExecutionStatus], None]) -> None:
        """
        添加执行状态回调
        
        Args:
            callback: 回调函数，接收 (node_id, status) 参数
        """
        self._callbacks.append(callback)
    
    def _notify_callbacks(self, node_id: str, status: ExecutionStatus) -> None:
        """
        通知所有回调
        
        Args:
            node_id: 节点ID
            status: 执行状态
        """
        for callback in self._callbacks:
            try:
                callback(node_id, status)
            except Exception as e:
                print(f"Callback error: {e}")
    
    def _propagate_outputs(self, node_id: str) -> None:
        """
        将节点的输出传播到连接的节点
        
        Args:
            node_id: 源节点ID
        """
        node = self.graph.get_node(node_id)
        if node is None:
            return
        
        # 获取节点的输出连接
        _, output_connections = self.graph.get_node_connections(node_id)
        
        # 传播输出到目标节点
        for conn in output_connections:
            target_node = self.graph.get_node(conn.to_node)
            if target_node is not None:
                output_value = node.get_output(conn.from_output)
                target_node.set_input(conn.to_input, output_value)
    
    def _execute_node(self, node_id: str) -> NodeExecutionResult:
        """
        执行单个节点
        
        Args:
            node_id: 节点ID
            
        Returns:
            节点执行结果
        """
        node = self.graph.get_node(node_id)
        if node is None:
            return NodeExecutionResult(
                node_id=node_id,
                status=ExecutionStatus.FAILED,
                error="Node not found"
            )
        
        result = NodeExecutionResult(
            node_id=node_id,
            status=ExecutionStatus.RUNNING,
            start_time=datetime.now()
        )
        
        try:
            # 通知开始执行
            self._notify_callbacks(node_id, ExecutionStatus.RUNNING)
            
            # 执行节点
            outputs = node.run()
            
            # 记录结果
            result.outputs = outputs
            result.status = ExecutionStatus.COMPLETED
            result.end_time = datetime.now()
            
            if result.start_time:
                duration = (result.end_time - result.start_time).total_seconds() * 1000
                result.duration_ms = duration
            
            # 传播输出
            self._propagate_outputs(node_id)
            
            # 通知完成
            self._notify_callbacks(node_id, ExecutionStatus.COMPLETED)
            
        except Exception as e:
            # 记录错误
            result.status = ExecutionStatus.FAILED
            result.error = str(e)
            result.end_time = datetime.now()
            
            if result.start_time:
                duration = (result.end_time - result.start_time).total_seconds() * 1000
                result.duration_ms = duration
            
            # 通知失败
            self._notify_callbacks(node_id, ExecutionStatus.FAILED)
        
        return result
    
    def execute(self) -> GraphExecutionResult:
        """
        执行整个节点图
        
        Returns:
            图执行结果
        """
        # 初始化结果
        self._execution_result = GraphExecutionResult(
            graph_id=self.graph.graph_id,
            status=ExecutionStatus.RUNNING,
            start_time=datetime.now()
        )
        
        try:
            # 验证图
            is_valid, error = self.graph.validate()
            if not is_valid:
                self._execution_result.status = ExecutionStatus.FAILED
                self._execution_result.error = f"Graph validation failed: {error}"
                self._execution_result.end_time = datetime.now()
                return self._execution_result
            
            # 获取拓扑排序
            execution_order = self.graph.topological_sort()
            if execution_order is None:
                self._execution_result.status = ExecutionStatus.FAILED
                self._execution_result.error = "Failed to get execution order (cycle detected)"
                self._execution_result.end_time = datetime.now()
                return self._execution_result
            
            # 按顺序执行节点
            for node_id in execution_order:
                node_result = self._execute_node(node_id)
                self._execution_result.node_results[node_id] = node_result
                
                # 如果节点执行失败，停止执行
                if node_result.status == ExecutionStatus.FAILED:
                    self._execution_result.status = ExecutionStatus.FAILED
                    self._execution_result.error = f"Node {node_id} failed: {node_result.error}"
                    break
            else:
                # 所有节点执行成功
                self._execution_result.status = ExecutionStatus.COMPLETED
            
        except Exception as e:
            # 记录异常
            self._execution_result.status = ExecutionStatus.FAILED
            self._execution_result.error = f"Execution error: {str(e)}"
        
        finally:
            # 记录结束时间
            self._execution_result.end_time = datetime.now()
            
            if self._execution_result.start_time:
                duration = (self._execution_result.end_time - self._execution_result.start_time).total_seconds() * 1000
                self._execution_result.duration_ms = duration
        
        return self._execution_result
    
    def get_result(self) -> Optional[GraphExecutionResult]:
        """
        获取执行结果
        
        Returns:
            执行结果，如果还未执行则返回 None
        """
        return self._execution_result
    
    def get_node_output(self, node_id: str, output_name: str) -> Any:
        """
        获取节点的输出值
        
        Args:
            node_id: 节点ID
            output_name: 输出名称
            
        Returns:
            输出值
        """
        node = self.graph.get_node(node_id)
        if node is None:
            return None
        return node.get_output(output_name)
    
    def get_all_outputs(self) -> Dict[str, Dict[str, Any]]:
        """
        获取所有节点的输出
        
        Returns:
            节点输出字典，格式为 {node_id: {output_name: value}}
        """
        outputs = {}
        for node_id, node in self.graph._nodes.items():
            outputs[node_id] = node.get_all_outputs()
        return outputs


class ExecutionCache:
    """
    执行结果缓存
    
    缓存图的执行结果，避免重复执行。
    """
    
    def __init__(self, max_size: int = 100):
        """
        初始化缓存
        
        Args:
            max_size: 最大缓存数量
        """
        self._cache: Dict[str, GraphExecutionResult] = {}
        self._max_size = max_size
    
    def put(self, graph_id: str, result: GraphExecutionResult) -> None:
        """
        存储执行结果
        
        Args:
            graph_id: 图ID
            result: 执行结果
        """
        # 如果缓存已满，删除最旧的项
        if len(self._cache) >= self._max_size:
            oldest_key = next(iter(self._cache))
            del self._cache[oldest_key]
        
        self._cache[graph_id] = result
    
    def get(self, graph_id: str) -> Optional[GraphExecutionResult]:
        """
        获取执行结果
        
        Args:
            graph_id: 图ID
            
        Returns:
            执行结果，如果不存在则返回 None
        """
        return self._cache.get(graph_id)
    
    def invalidate(self, graph_id: str) -> bool:
        """
        使缓存失效
        
        Args:
            graph_id: 图ID
            
        Returns:
            是否成功使缓存失效
        """
        if graph_id in self._cache:
            del self._cache[graph_id]
            return True
        return False
    
    def clear(self) -> None:
        """清空缓存"""
        self._cache.clear()
    
    def __len__(self) -> int:
        """返回缓存项数量"""
        return len(self._cache)

