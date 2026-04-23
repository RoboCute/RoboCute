from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import (
    uint,
    float3,
    float4,
    Vector,
    VoidPtr,
)
from enum import Enum


@reflect(
    cpp_namespace="rbc::editor",
    module_name="editor_interface",
    pybind=True,
)
class EditorSceneSourceType(Enum):
    NONE = 0
    SERVER = 1
    LOCAL = 2


@reflect(
    cpp_namespace="rbc::editor",
    serde=True,
)
class SceneTransform:
    position: float3
    rotation: float4
    scale: float3

    _cpp_init = {
        "position": "0.0f, 0.0f, 0.0f",
        "rotation": "0.0f, 0.0f, 0.0f, 1.0f",
        "scale": "1.0f, 1.0f, 1.0f",
    }


@reflect(
    cpp_namespace="rbc::editor",
    serde=True,
)
class ServerEntityData:
    entity_id: int
    name: str
    mesh_path: str
    transform: SceneTransform
    has_render_component: bool

    _cpp_init = {
        "entity_id": "0",
        "has_render_component": "false",
    }


@reflect(
    cpp_namespace="rbc::editor",
    serde=True,
)
class SceneSyncData:
    entities: Vector[ServerEntityData]
    has_changes: bool

    _cpp_init = {
        "has_changes": "false",
    }


@reflect(
    cpp_namespace="rbc::editor",
    serde=True,
)
class SceneConfig:
    scene_name: str
    scene_file: str
    source_type: EditorSceneSourceType
    server_url: str
    is_default: bool

    _cpp_init = {
        "source_type": "rbc::editor::EditorSceneSourceType::LOCAL",
        "is_default": "false",
    }


@reflect(
    pybind=True,
    cpp_prefix="RBC_EDITOR_RUNTIME_API",
    cpp_namespace="rbc::editor",
)
class EditorScene:
    def initFromServer(serverUrl: str) -> bool: ...
    def initFromLocal(projectPath: str) -> bool: ...
    def shutdown() -> None: ...
    def sourceType() -> EditorSceneSourceType: ...
    def isReady() -> bool: ...
    def isWorldInitialized() -> bool: ...
    def onFrameTick() -> None: ...
    def updateFromSync(syncData: SceneSyncData) -> None: ...
    def getEntity(localId: int) -> VoidPtr: ...
    def getAllEntityIds() -> Vector[int]: ...
    def entityCount() -> uint: ...
    def getEntityIdFromInstanceId(instanceId: uint) -> int: ...
    def getEntityTransform(localId: int) -> SceneTransform: ...
    def setEntityTransform(localId: int, transform: SceneTransform) -> None: ...
    def createEntity(name: str) -> int: ...
    def deleteEntity(localId: int) -> bool: ...
    def saveScene() -> bool: ...


@reflect(
    pybind=True,
    cpp_prefix="RBC_EDITOR_RUNTIME_API",
    cpp_namespace="rbc::editor",
)
class EditorProject:
    def load(projectPath: str) -> bool: ...
    def close() -> None: ...
    def isOpen() -> bool: ...
    def projectRoot() -> str: ...
    def projectName() -> str: ...
    def scenes() -> Vector[SceneConfig]: ...
    def defaultScene() -> SceneConfig: ...
    def findScene(name: str) -> SceneConfig: ...
    def addScene(config: SceneConfig) -> None: ...
    def removeScene(name: str) -> bool: ...
    def activeScene() -> EditorScene: ...
    def openScene(name: str) -> bool: ...
    def closeActiveScene() -> None: ...


# OUT_CLASSES = [
#     EditorSceneSourceType,
#     SceneTransform,
#     ServerEntityData,
#     SceneSyncData,
#     SceneConfig,
#     EditorScene,
#     EditorProject,
# ]

# __all__ = ["OUT_CLASSES"]
