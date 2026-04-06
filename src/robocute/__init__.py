from robocute.animation import AnimationKeyframe, AnimationSequence, AnimationClip
from robocute.scene import Scene, Model
import os
from .rbc_ext import world


import robocute.app as app

__version__ = "0.2.3"
__author__ = "RoboCute Team"
__builtin_runtime_dir__ = (
    os.path.dirname(__file__) + "/rbc_ext/_C"
)  # Built-In Runtime Path

__all__ = [
    # Project
    "world",
    "app",
    # Scene
    "Scene",
    "Entity",
    "TransformComponent",
    "RenderComponent",
    # Animation
    "AnimationKeyframe",
    "AnimationSequence",
    "AnimationClip",
    # Scene Context
    "SceneContext",
    # Service & Server
    "Service",
    "Server",
    "NodeGraphService",
    # Resource Management
    "ResourceManager",
    "ResourceHandle",
    "ResourceType",
    "ResourceState",
    "LoadPriority",
    "LoadOptions",
    # Resource Utilities
    "create_default_material",
    "load_scene_resources",
    "preload_with_lod",
    "batch_load_resources",
    "manage_memory_budget",
    "ResourceBatch",
    "wait_for_resources",
    # Editor
    "EditorService",
    "EditorCommand",
    # 节点基类
    "RBCNode",
    "NodeInput",
    "NodeOutput",
    "NodeMetadata",
    # 注册系统
    "NodeRegistry",
    "register_node",
    "get_registry",
    # 图相关
    "NodeGraph",
    "NodeConnection",
    "NodeDefinition",
    "GraphDefinition",
    # 执行器
    "GraphExecutor",
    "ExecutionStatus",
    "NodeExecutionResult",
    "GraphExecutionResult",
    "ExecutionCache",
    # 模块信息
    "__version__",
    "__author__",
    "__builtin_runtime_dir__",
]
