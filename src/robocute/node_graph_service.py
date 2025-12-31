"""
NodeGraphService - Node Graph API Service

Provides HTTP API for node management, graph creation and execution.
"""

from typing import Dict, Any, List, Optional
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import uuid
from datetime import datetime

from .node_registry import get_registry
from .graph import NodeGraph, GraphDefinition
from .executor import GraphExecutor, ExecutionStatus, GraphExecutionResult
from .context import SceneContext
from .scene import Scene
from .service import Service


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


class NodeGraphService(Service):
    """
    Node Graph API Service
    
    Provides HTTP endpoints for node management, graph creation and execution.
    """

    def __init__(self, scene: Optional[Scene] = None):
        """
        Initialize NodeGraphService
        
        Args:
            scene: Optional scene reference for graph execution context
        """
        super().__init__("NodeGraphService")
        self.scene = scene
        
        # In-memory storage
        self._graphs: Dict[str, NodeGraph] = {}
        self._execution_results: Dict[str, GraphExecutionResult] = {}
    
    def set_scene(self, scene: Scene) -> None:
        """
        Set the scene reference for graph execution context.
        
        Args:
            scene: Scene instance
        """
        self.scene = scene
    
    def register_routes(self, app: FastAPI) -> None:
        """Register Node Graph API routes with the FastAPI application"""
        
        @app.get("/")
        async def root():
            """根路径"""
            return {
                "name": "RBCNode API",
                "version": "0.2.0",
                "endpoints": {
                    "nodes": "/nodes",
                    "graph_create": "/graph/create",
                    "graph_execute": "/graph/execute",
                    "graph_status": "/graph/{graph_id}/status",
                },
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
                    "outputs": [output_def.model_dump() for output_def in meta.outputs],
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
                raise HTTPException(
                    status_code=404, detail=f"Node type '{node_type}' not found"
                )

            return {
                "node_type": metadata.node_type,
                "display_name": metadata.display_name,
                "category": metadata.category,
                "description": metadata.description,
                "inputs": [input_def.model_dump() for input_def in metadata.inputs],
                "outputs": [output_def.model_dump() for output_def in metadata.outputs],
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
                if graph_id in self._graphs:
                    raise HTTPException(
                        status_code=400, detail=f"Graph with id '{graph_id}' already exists"
                    )

                # 创建图
                graph = NodeGraph.from_definition(request.graph_definition, graph_id)

                # 验证图
                is_valid, error = graph.validate()
                if not is_valid:
                    raise HTTPException(status_code=400, detail=f"Invalid graph: {error}")

                # 存储图
                self._graphs[graph_id] = graph

                return GraphCreateResponse(
                    graph_id=graph_id,
                    message=f"Graph created successfully with {len(graph)} nodes",
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
            print("\n" + "=" * 60)
            print("[API] Graph execution request received")
            print("=" * 60)

            try:
                graph = None
                execution_id = None

                if request.graph_id:
                    # 执行已创建的图
                    if request.graph_id not in self._graphs:
                        raise HTTPException(
                            status_code=404, detail=f"Graph '{request.graph_id}' not found"
                        )

                    graph = self._graphs[request.graph_id]
                    execution_id = request.graph_id

                elif request.graph_definition:
                    # 临时创建并执行图
                    execution_id = f"exec_{uuid.uuid4()}"
                    print(f"[API] Creating graph from definition: {execution_id}")
                    print(f"[API] Nodes: {len(request.graph_definition.nodes)}")
                    print(f"[API] Connections: {len(request.graph_definition.connections)}")

                    # Log node details
                    for node_def in request.graph_definition.nodes:
                        print(f"  Node: {node_def.node_id} (type: {node_def.node_type})")
                        print(f"    Inputs: {node_def.inputs}")

                    # Create scene context BEFORE creating graph
                    if self.scene:
                        print(
                            f"[API] Creating scene context with {len(self.scene.get_all_entities())} entities"
                        )
                        scene_context = SceneContext(self.scene)
                    else:
                        print("[API] WARNING: No scene available, graph will have no context")
                        scene_context = None

                    # Create graph WITH scene context
                    graph = NodeGraph.from_definition(
                        request.graph_definition, execution_id, scene_context
                    )

                    # 验证图
                    print("[API] Validating graph...")
                    is_valid, error = graph.validate()
                    if not is_valid:
                        print(f"[API] ✗ Graph validation failed: {error}")
                        raise HTTPException(status_code=400, detail=f"Invalid graph: {error}")
                    print("[API] ✓ Graph validation passed")
                else:
                    raise HTTPException(
                        status_code=400,
                        detail="Either graph_id or graph_definition must be provided",
                    )

                # Note: scene_context should already be set in graph at this point
                # For graph_id case, we need to create it here
                if request.graph_id:
                    if self.scene:
                        print(
                            f"[API] Creating scene context for existing graph with {len(self.scene.get_all_entities())} entities"
                        )
                        scene_context = SceneContext(self.scene)
                    else:
                        print("[API] WARNING: No scene available for existing graph")
                        scene_context = None
                else:
                    # For graph_definition case, scene_context was already created and passed to graph
                    scene_context = graph.scene_context
                    print(f"[API] Using scene context from graph: {scene_context is not None}")

                print("[API] Creating executor...")
                executor = GraphExecutor(graph, scene_context)

                print("[API] Starting graph execution...")
                result = executor.execute()

                print(f"[API] Execution finished: {result.status.value}")
                if result.error:
                    print(f"[API] Error: {result.error}")

                # 存储执行结果
                self._execution_results[execution_id] = result

                # Log node results
                for node_id, node_result in result.node_results.items():
                    status_icon = (
                        "✓" if node_result.status == ExecutionStatus.COMPLETED else "✗"
                    )
                    print(f"  {status_icon} Node '{node_id}': {node_result.status.value}")
                    if node_result.error:
                        print(f"      Error: {node_result.error}")

                print("=" * 60 + "\n")

                return GraphExecuteResponse(
                    execution_id=execution_id,
                    status=result.status.value,
                    message=f"Graph execution {'completed' if result.status == ExecutionStatus.COMPLETED else 'failed'}",
                )

            except ValueError as e:
                print(f"[API] ValueError during graph execution: {e}")
                import traceback

                traceback.print_exc()
                raise HTTPException(status_code=400, detail=str(e))
            except Exception as e:
                print(f"[API] Exception during graph execution: {e}")
                import traceback

                traceback.print_exc()
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
            if graph_id not in self._execution_results:
                raise HTTPException(
                    status_code=404, detail=f"Execution result for '{graph_id}' not found"
                )

            result = self._execution_results[graph_id]

            return StatusResponse(status=result.status.value, result=result.model_dump())

        @app.get("/graph/{graph_id}/outputs")
        async def get_graph_outputs(graph_id: str):
            """
            获取图执行的输出结果

            Args:
                graph_id: 图ID

            Returns:
                输出结果
            """
            if graph_id not in self._execution_results:
                raise HTTPException(
                    status_code=404, detail=f"Execution result for '{graph_id}' not found"
                )

            result = self._execution_results[graph_id]

            # 提取所有节点的输出
            outputs = {}
            for node_id, node_result in result.node_results.items():
                outputs[node_id] = node_result.outputs

            return {"graph_id": graph_id, "status": result.status.value, "outputs": outputs}

        @app.delete("/graph/{graph_id}")
        async def delete_graph(graph_id: str):
            """
            删除图

            Args:
                graph_id: 图ID

            Returns:
                删除结果
            """
            if graph_id not in self._graphs:
                raise HTTPException(status_code=404, detail=f"Graph '{graph_id}' not found")

            del self._graphs[graph_id]

            # 同时删除执行结果
            if graph_id in self._execution_results:
                del self._execution_results[graph_id]

            return {"message": f"Graph '{graph_id}' deleted successfully"}

        @app.get("/graphs")
        async def list_graphs():
            """
            列出所有已创建的图

            Returns:
                图列表
            """
            graphs_info = []
            for graph_id, graph in self._graphs.items():
                graphs_info.append(
                    {
                        "graph_id": graph_id,
                        "node_count": len(graph),
                        "connection_count": len(graph.get_connections()),
                    }
                )

            return {"graphs": graphs_info, "count": len(graphs_info)}

        @app.get("/executions")
        async def list_executions():
            """
            列出所有执行结果

            Returns:
                执行结果列表
            """
            executions = []
            for exec_id, result in self._execution_results.items():
                executions.append(
                    {
                        "execution_id": exec_id,
                        "status": result.status.value,
                        "start_time": result.start_time.isoformat()
                        if result.start_time
                        else None,
                        "end_time": result.end_time.isoformat() if result.end_time else None,
                        "duration_ms": result.duration_ms,
                    }
                )

            return {"executions": executions, "count": len(executions)}

        @app.post("/clear")
        async def clear_all():
            """
            清空所有图和执行结果

            Returns:
                清空结果
            """
            self._graphs.clear()
            self._execution_results.clear()

            return {"message": "All graphs and execution results cleared"}

        # 健康检查
        @app.get("/health")
        async def health_check():
            """健康检查"""
            return {
                "status": "healthy",
                "timestamp": datetime.now().isoformat(),
                "graphs_count": len(self._graphs),
                "executions_count": len(self._execution_results),
                "registered_nodes": len(get_registry()),
            }

        # === Animation Endpoints ===

        @app.get("/animations")
        async def get_animations():
            """
            Get all animations in the scene

            Returns:
                List of animation names and metadata
            """
            if self.scene is None:
                raise HTTPException(status_code=503, detail="Scene not initialized")

            try:
                animations = self.scene.get_all_animations()
                result = []

                for name, clip in animations.items():
                    result.append(
                        {
                            "name": clip.name,
                            "total_frames": clip.get_total_frames(),
                            "fps": clip.fps,
                            "duration_seconds": clip.get_duration_seconds(),
                            "num_sequences": len(clip.sequences),
                        }
                    )

                return {"animations": result, "count": len(result)}
            except Exception as e:
                raise HTTPException(
                    status_code=500, detail=f"Error fetching animations: {str(e)}"
                )

        @app.get("/animations/{name}")
        async def get_animation(name: str):
            """
            Get specific animation data

            Args:
                name: Animation name

            Returns:
                Full animation clip data
            """
            if self.scene is None:
                raise HTTPException(status_code=503, detail="Scene not initialized")

            try:
                clip = self.scene.get_animation(name)
                if clip is None:
                    raise HTTPException(status_code=404, detail=f"Animation '{name}' not found")

                return {"animation": clip.to_dict()}
            except HTTPException:
                raise
            except Exception as e:
                raise HTTPException(
                    status_code=500, detail=f"Error fetching animation: {str(e)}"
                )

