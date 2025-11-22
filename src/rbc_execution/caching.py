"""
缓存系统

提供执行结果的缓存功能。
"""
from typing import Dict, Optional
from .executor import GraphExecutionResult


class ExecutionCache:
    """
    执行结果缓存
    
    缓存图的执行结果，避免重复执行。
    注意：此类已在 executor.py 中定义，这里重新导出以保持向后兼容。
    """
    
    def __init__(self, max_size: int = 100):
        """
        初始化缓存
        
        Args:
            max_size: 最大缓存数量
        """
        self._cache: Dict[str, GraphExecutionResult] = {}
        self._max_size = max_size
        self._access_order: list = []  # 用于 LRU
    
    def put(self, graph_id: str, result: GraphExecutionResult) -> None:
        """
        存储执行结果
        
        Args:
            graph_id: 图ID
            result: 执行结果
        """
        # 如果缓存已满，删除最旧的项 (LRU)
        if len(self._cache) >= self._max_size and graph_id not in self._cache:
            if self._access_order:
                oldest_key = self._access_order.pop(0)
                if oldest_key in self._cache:
                    del self._cache[oldest_key]
        
        # 更新缓存
        self._cache[graph_id] = result
        
        # 更新访问顺序
        if graph_id in self._access_order:
            self._access_order.remove(graph_id)
        self._access_order.append(graph_id)
    
    def get(self, graph_id: str) -> Optional[GraphExecutionResult]:
        """
        获取执行结果
        
        Args:
            graph_id: 图ID
            
        Returns:
            执行结果，如果不存在则返回 None
        """
        result = self._cache.get(graph_id)
        
        # 更新访问顺序 (LRU)
        if result is not None and graph_id in self._access_order:
            self._access_order.remove(graph_id)
            self._access_order.append(graph_id)
        
        return result
    
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
            if graph_id in self._access_order:
                self._access_order.remove(graph_id)
            return True
        return False
    
    def clear(self) -> None:
        """清空缓存"""
        self._cache.clear()
        self._access_order.clear()
    
    def get_stats(self) -> Dict[str, int]:
        """
        获取缓存统计信息
        
        Returns:
            统计信息字典
        """
        return {
            "size": len(self._cache),
            "max_size": self._max_size,
            "utilization": int(len(self._cache) / self._max_size * 100) if self._max_size > 0 else 0
        }
    
    def __len__(self) -> int:
        """返回缓存项数量"""
        return len(self._cache)
    
    def __contains__(self, graph_id: str) -> bool:
        """检查图ID是否在缓存中"""
        return graph_id in self._cache


# 全局缓存实例
_global_cache = ExecutionCache()


def get_cache() -> ExecutionCache:
    """
    获取全局缓存实例
    
    Returns:
        全局缓存
    """
    return _global_cache

