"""
FastAPI Web 接口

提供 HTTP API 用于节点管理、图创建和执行。
"""
from typing import Dict, Any, List, Optional
from fastapi import FastAPI, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel
import uuid
from datetime import datetime

from .node_registry import get_registry
from .graph import NodeGraph, GraphDefinition
from .executor import GraphExecutor, ExecutionStatus, GraphExecutionResult
from .scene_context import SceneContext


# 创建 FastAPI 应用
app = FastAPI(
    title="RBCNode API",
    description="类似 ComfyUI 的节点系统后端 API",
    version="0.1.0"
)


# 内存存储
_graphs: Dict[str, NodeGraph] = {}
_execution_results: Dict[str, GraphExecutionResult] = {}
_scene: Optional[Any] = None  # Global scene reference for animation storage


# API 数据模型
class NodeListResponse(BaseModel):
    """节点列表响应"""
    nodes: List[Dict[str, Any]]
    count: int


class GraphCreateRequest(BaseModel):
    """创建图请求"""
    graph_definition: GraphDefinition
    graph_id: Optional[str] = None


class GraphCreateResponse(BaseModel):
    """创建图响应"""
    graph_id: str
    message: str


class GraphExecuteRequest(BaseModel):
    """执行图请求"""
    graph_definition: Optional[GraphDefinition] = None
    graph_id: Optional[str] = None


class GraphExecuteResponse(BaseModel):
    """执行图响应"""
    execution_id: str
    status: str
    message: str


class StatusResponse(BaseModel):
    """状态响应"""
    status: str
    result: Optional[Dict[str, Any]] = None


# API 路由
@app.get("/")
async def root():
    """根路径"""
    return {
        "name": "RBCNode API",
        "version": "0.1.0",
        "endpoints": {
            "nodes": "/nodes",
            "graph_create": "/graph/create",
            "graph_execute": "/graph/execute",
            "graph_status": "/graph/{graph_id}/status"
        }
    }


@app.get("/nodes", response_model=NodeListResponse)
async def get_nodes():
    """
    获取所有可用节点定义
    
    Returns:
        节点列表
    """
    registry = get_registry()
    metadata_list = registry.get_all_metadata()
    
    nodes = [
        {
            "node_type": meta.node_type,
            "display_name": meta.display_name,
            "category": meta.category,
            "description": meta.description,
            "inputs": [input_def.model_dump() for input_def in meta.inputs],
            "outputs": [output_def.model_dump() for output_def in meta.outputs]
        }
        for meta in metadata_list
    ]
    
    return NodeListResponse(nodes=nodes, count=len(nodes))


@app.get("/nodes/{node_type}")
async def get_node_info(node_type: str):
    """
    获取指定节点的详细信息
    
    Args:
        node_type: 节点类型
        
    Returns:
        节点信息
    """
    registry = get_registry()
    metadata = registry.get_metadata(node_type)
    
    if metadata is None:
        raise HTTPException(status_code=404, detail=f"Node type '{node_type}' not found")
    
    return {
        "node_type": metadata.node_type,
        "display_name": metadata.display_name,
        "category": metadata.category,
        "description": metadata.description,
        "inputs": [input_def.model_dump() for input_def in metadata.inputs],
        "outputs": [output_def.model_dump() for output_def in metadata.outputs]
    }


@app.post("/graph/create", response_model=GraphCreateResponse)
async def create_graph(request: GraphCreateRequest):
    """
    创建节点图
    
    Args:
        request: 创建图请求
        
    Returns:
        创建结果
    """
    try:
        # 生成或使用提供的图ID
        graph_id = request.graph_id or str(uuid.uuid4())
        
        # 检查图ID是否已存在
        if graph_id in _graphs:
            raise HTTPException(status_code=400, detail=f"Graph with id '{graph_id}' already exists")
        
        # 创建图
        graph = NodeGraph.from_definition(request.graph_definition, graph_id)
        
        # 验证图
        is_valid, error = graph.validate()
        if not is_valid:
            raise HTTPException(status_code=400, detail=f"Invalid graph: {error}")
        
        # 存储图
        _graphs[graph_id] = graph
        
        return GraphCreateResponse(
            graph_id=graph_id,
            message=f"Graph created successfully with {len(graph)} nodes"
        )
        
    except ValueError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Internal error: {str(e)}")


@app.post("/graph/execute", response_model=GraphExecuteResponse)
async def execute_graph(request: GraphExecuteRequest):
    """
    执行节点图
    
    可以执行已创建的图，或者直接提供图定义执行。
    
    Args:
        request: 执行图请求
        
    Returns:
        执行结果
    """
    try:
        graph = None
        execution_id = None
        
        if request.graph_id:
            # 执行已创建的图
            if request.graph_id not in _graphs:
                raise HTTPException(status_code=404, detail=f"Graph '{request.graph_id}' not found")
            
            graph = _graphs[request.graph_id]
            execution_id = request.graph_id
            
        elif request.graph_definition:
            # 临时创建并执行图
            execution_id = f"exec_{uuid.uuid4()}"
            graph = NodeGraph.from_definition(request.graph_definition, execution_id)
            
            # 验证图
            is_valid, error = graph.validate()
            if not is_valid:
                raise HTTPException(status_code=400, detail=f"Invalid graph: {error}")
        else:
            raise HTTPException(status_code=400, detail="Either graph_id or graph_definition must be provided")
        
        # 执行图 with scene context if available
        scene_context = SceneContext(_scene) if _scene else None
        executor = GraphExecutor(graph, scene_context)
        result = executor.execute()
        
        # 存储执行结果
        _execution_results[execution_id] = result
        
        return GraphExecuteResponse(
            execution_id=execution_id,
            status=result.status.value,
            message=f"Graph execution {'completed' if result.status == ExecutionStatus.COMPLETED else 'failed'}"
        )
        
    except ValueError as e:
        raise HTTPException(status_code=400, detail=str(e))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Internal error: {str(e)}")


@app.get("/graph/{graph_id}/status", response_model=StatusResponse)
async def get_graph_status(graph_id: str):
    """
    查询图执行状态
    
    Args:
        graph_id: 图ID或执行ID
        
    Returns:
        执行状态
    """
    if graph_id not in _execution_results:
        raise HTTPException(status_code=404, detail=f"Execution result for '{graph_id}' not found")
    
    result = _execution_results[graph_id]
    
    return StatusResponse(
        status=result.status.value,
        result=result.model_dump()
    )


@app.get("/graph/{graph_id}/outputs")
async def get_graph_outputs(graph_id: str):
    """
    获取图执行的输出结果
    
    Args:
        graph_id: 图ID
        
    Returns:
        输出结果
    """
    if graph_id not in _execution_results:
        raise HTTPException(status_code=404, detail=f"Execution result for '{graph_id}' not found")
    
    result = _execution_results[graph_id]
    
    # 提取所有节点的输出
    outputs = {}
    for node_id, node_result in result.node_results.items():
        outputs[node_id] = node_result.outputs
    
    return {
        "graph_id": graph_id,
        "status": result.status.value,
        "outputs": outputs
    }


@app.delete("/graph/{graph_id}")
async def delete_graph(graph_id: str):
    """
    删除图
    
    Args:
        graph_id: 图ID
        
    Returns:
        删除结果
    """
    if graph_id not in _graphs:
        raise HTTPException(status_code=404, detail=f"Graph '{graph_id}' not found")
    
    del _graphs[graph_id]
    
    # 同时删除执行结果
    if graph_id in _execution_results:
        del _execution_results[graph_id]
    
    return {"message": f"Graph '{graph_id}' deleted successfully"}


@app.get("/graphs")
async def list_graphs():
    """
    列出所有已创建的图
    
    Returns:
        图列表
    """
    graphs_info = []
    for graph_id, graph in _graphs.items():
        graphs_info.append({
            "graph_id": graph_id,
            "node_count": len(graph),
            "connection_count": len(graph.get_connections())
        })
    
    return {
        "graphs": graphs_info,
        "count": len(graphs_info)
    }


@app.get("/executions")
async def list_executions():
    """
    列出所有执行结果
    
    Returns:
        执行结果列表
    """
    executions = []
    for exec_id, result in _execution_results.items():
        executions.append({
            "execution_id": exec_id,
            "status": result.status.value,
            "start_time": result.start_time.isoformat() if result.start_time else None,
            "end_time": result.end_time.isoformat() if result.end_time else None,
            "duration_ms": result.duration_ms
        })
    
    return {
        "executions": executions,
        "count": len(executions)
    }


@app.post("/clear")
async def clear_all():
    """
    清空所有图和执行结果
    
    Returns:
        清空结果
    """
    _graphs.clear()
    _execution_results.clear()
    
    return {"message": "All graphs and execution results cleared"}


# 健康检查
@app.get("/health")
async def health_check():
    """健康检查"""
    return {
        "status": "healthy",
        "timestamp": datetime.now().isoformat(),
        "graphs_count": len(_graphs),
        "executions_count": len(_execution_results),
        "registered_nodes": len(get_registry())
    }


# === Animation Endpoints ===

def set_scene(scene):
    """Set the global scene reference for API access"""
    global _scene
    _scene = scene


@app.get("/animations")
async def get_animations():
    """
    Get all animations in the scene
    
    Returns:
        List of animation names and metadata
    """
    if _scene is None:
        raise HTTPException(status_code=503, detail="Scene not initialized")
    
    try:
        animations = _scene.get_all_animations()
        result = []
        
        for name, clip in animations.items():
            result.append({
                "name": clip.name,
                "total_frames": clip.get_total_frames(),
                "fps": clip.fps,
                "duration_seconds": clip.get_duration_seconds(),
                "num_sequences": len(clip.sequences)
            })
        
        return {
            "animations": result,
            "count": len(result)
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error fetching animations: {str(e)}")


@app.get("/animations/{name}")
async def get_animation(name: str):
    """
    Get specific animation data
    
    Args:
        name: Animation name
        
    Returns:
        Full animation clip data
    """
    if _scene is None:
        raise HTTPException(status_code=503, detail="Scene not initialized")
    
    try:
        clip = _scene.get_animation(name)
        if clip is None:
            raise HTTPException(status_code=404, detail=f"Animation '{name}' not found")
        
        return {
            "animation": clip.to_dict()
        }
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error fetching animation: {str(e)}")

