"""
EditorService - Server-Editor Communication and Resource Synchronization

Manages communication between Python Server and C++ Editor,
including resource synchronization and command handling.
"""

from typing import Optional, Dict, List, Any
import threading
import queue
import time
from dataclasses import dataclass
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import uvicorn
from rbc_ext.resource import ResourceManager, ResourceType, LoadPriority
from .scene import Scene


@dataclass
class EditorCommand:
    """Command from Editor to Server"""

    type: str
    params: Dict[str, Any]
    timestamp: float
    editor_id: str = ""


class HeartbeatRequest(BaseModel):
    """Heartbeat request from editor"""

    editor_id: str


class RegisterRequest(BaseModel):
    """Editor registration request"""

    editor_id: str


class EditorCommandRequest(BaseModel):
    """Editor command request"""

    editor_id: str
    command_type: str
    params: Dict[str, Any]


class EditorConnection:
    """Represents a connected editor instance"""

    def __init__(self, editor_id: str):
        self.id = editor_id
        self.connected_at = time.time()
        self.last_heartbeat = time.time()
        self.message_queue: queue.Queue = queue.Queue()

    def send_message(self, message: dict):
        """Queue a message to be sent to this editor"""
        self.message_queue.put(message)

    def is_alive(self, timeout: float = 10.0) -> bool:
        """Check if editor connection is still alive"""
        return (time.time() - self.last_heartbeat) < timeout


class EditorService:
    """
    Editor Service for Python Server

    Responsibilities:
    - Manage connections from C++ Editor instances
    - Sync resource metadata to editors
    - Handle editor commands (resource loading, scene modifications)
    - Broadcast state changes
    """

    def __init__(
        self, scene: Scene, resource_manager: Optional[ResourceManager] = None
    ):
        """
        Initialize EditorService

        Args:
            scene: The scene to manage
            resource_manager: Resource manager (uses scene's if None)
        """
        self.scene = scene
        self.resource_manager = resource_manager or scene.resource_manager

        # Editor connections
        self._editors: Dict[str, EditorConnection] = {}
        self._editors_lock = threading.RLock()

        # Command queue
        self._command_queue: queue.Queue = queue.Queue()

        # Service thread
        self._running = False
        self._service_thread: Optional[threading.Thread] = None
        self._http_server_thread: Optional[threading.Thread] = None

        # Configuration
        self.port = 5555
        self.update_rate_hz = 30  # State push frequency

        # HTTP Server
        self._app: Optional[FastAPI] = None
        self._setup_http_server()

    # === HTTP Server Setup ===

    def _setup_http_server(self):
        """Setup FastAPI HTTP server"""
        self._app = FastAPI(title="RoboCute Editor Service")

        @self._app.get("/scene/state")
        def get_scene_state():
            """Get current scene state"""
            try:
                scene_data = self.scene._save_to_dict()

                # print(f"[EditorService] Scene state: {json_lib.dumps(scene_data, indent=2)}")
                return {"success": True, "scene": scene_data}
            except Exception as e:
                raise HTTPException(status_code=500, detail=str(e))

        @self._app.get("/resources/all")
        def get_all_resources():
            """Get all resource metadata"""
            try:
                all_metadata = self.resource_manager.get_all_metadata()
                resources = [
                    self.resource_manager.serialize_metadata(meta.id)
                    for meta in all_metadata
                ]
                return {"success": True, "resources": resources}
            except Exception as e:
                raise HTTPException(status_code=500, detail=str(e))

        @self._app.get("/resources/{resource_id}")
        def get_resource(resource_id: int):
            """Get specific resource metadata"""
            try:
                metadata = self.resource_manager.serialize_metadata(resource_id)
                if not metadata:
                    raise HTTPException(status_code=404, detail="Resource not found")
                return {"success": True, "resource": metadata}
            except Exception as e:
                raise HTTPException(status_code=500, detail=str(e))

        @self._app.post("/editor/register")
        def register_editor(request: RegisterRequest):
            """Register a new editor"""
            try:
                success = self.register_editor(request.editor_id)
                return {"success": success, "editor_id": request.editor_id}
            except Exception as e:
                raise HTTPException(status_code=500, detail=str(e))

        @self._app.post("/editor/heartbeat")
        def editor_heartbeat(request: HeartbeatRequest):
            """Editor heartbeat"""
            try:
                with self._editors_lock:
                    if request.editor_id in self._editors:
                        self._editors[request.editor_id].last_heartbeat = time.time()
                        return {"success": True}
                    else:
                        raise HTTPException(
                            status_code=404, detail="Editor not registered"
                        )
            except Exception as e:
                raise HTTPException(status_code=500, detail=str(e))

        @self._app.get("/animations/all")
        def get_all_animations():
            """Get all animations in the scene"""
            try:
                animations = self.scene.get_all_animations()
                result = []

                for name, clip in animations.items():
                    result.append(clip.to_dict())

                return {"success": True, "animations": result}
            except Exception as e:
                raise HTTPException(status_code=500, detail=str(e))

        @self._app.post("/editor/command")
        def editor_command(request: EditorCommandRequest):
            """Handle editor command (for direct HTTP requests)"""
            try:
                print(f"[EditorService] Received command: {request.command_type} from editor {request.editor_id}")
                print(f"[EditorService] Command params: {request.params}")
                
                command = EditorCommand(
                    type=request.command_type,
                    params=request.params,
                    timestamp=time.time(),
                    editor_id=request.editor_id,
                )
                self.submit_command(command)
                
                # Process command immediately
                self.process_commands()
                
                print(f"[EditorService] Command {request.command_type} processed successfully")
                return {"success": True, "command_type": request.command_type}
            except Exception as e:
                print(f"[EditorService] Error processing command {request.command_type}: {e}")
                import traceback
                traceback.print_exc()
                raise HTTPException(status_code=500, detail=str(e))

    # === Service Lifecycle ===

    def start(self, port: int = 5555):
        """Start the editor service"""
        if self._running:
            return

        self.port = port
        self._running = True

        # Start HTTP server in separate thread
        self._http_server_thread = threading.Thread(
            target=self._run_http_server, name="EditorHTTPServer", daemon=True
        )
        self._http_server_thread.start()

        # Start service thread
        self._service_thread = threading.Thread(
            target=self._service_loop, name="EditorService", daemon=True
        )
        self._service_thread.start()

        print(f"[EditorService] Started on port {port}")

    def _run_http_server(self):
        """Run the HTTP server"""
        config = uvicorn.Config(
            self._app,
            host="127.0.0.1",
            port=self.port,
            log_level="info",
            access_log=False,
        )
        server = uvicorn.Server(config)
        server.run()

    def stop(self):
        """Stop the editor service"""
        if not self._running:
            return

        self._running = False

        # Disconnect all editors
        with self._editors_lock:
            for editor in self._editors.values():
                self._send_to_editor(editor.id, {"event": "server_shutdown"})
            self._editors.clear()

        # Join service thread
        if self._service_thread and self._service_thread.is_alive():
            self._service_thread.join(timeout=2.0)

        # Note: HTTP server runs in daemon thread and will stop automatically
        # when main thread exits

        print("[EditorService] Stopped")

    # === Editor Connection Management ===

    def register_editor(self, editor_id: str) -> bool:
        """Register a new editor connection"""
        with self._editors_lock:
            if editor_id in self._editors:
                print(f"[EditorService] Editor {editor_id} already registered")
                return False

            connection = EditorConnection(editor_id)
            self._editors[editor_id] = connection

        # Send full sync to new editor
        self.sync_all_resources(editor_id)
        self._sync_scene_state(editor_id)

        print(f"[EditorService] Editor {editor_id} registered")
        return True

    def unregister_editor(self, editor_id: str):
        """Unregister an editor connection"""
        with self._editors_lock:
            if editor_id in self._editors:
                del self._editors[editor_id]
                print(f"[EditorService] Editor {editor_id} unregistered")

    def get_connected_editors(self) -> List[str]:
        """Get list of connected editor IDs"""
        with self._editors_lock:
            return list(self._editors.keys())

    # === Resource Synchronization ===

    def broadcast_resource_registered(self, resource_id: int):
        """Broadcast resource registration event to all editors"""
        metadata = self.resource_manager.serialize_metadata(resource_id)
        if not metadata:
            return

        message = {"event": "resource_registered", "resource": metadata}
        self._broadcast(message)

    def broadcast_resource_loaded(self, resource_id: int):
        """Broadcast resource loaded event to all editors"""
        metadata = self.resource_manager.serialize_metadata(resource_id)
        if not metadata:
            return

        message = {"event": "resource_loaded", "resource": metadata}
        self._broadcast(message)

    def broadcast_resource_unloaded(self, resource_id: int):
        """Broadcast resource unloaded event to all editors"""
        message = {"event": "resource_unloaded", "resource_id": resource_id}
        self._broadcast(message)

    def sync_all_resources(self, editor_id: str):
        """Synchronize all resource metadata to a specific editor"""
        all_metadata = self.resource_manager.get_all_metadata()

        resources = [
            self.resource_manager.serialize_metadata(meta.id) for meta in all_metadata
        ]

        message = {"event": "resources_full_sync", "resources": resources}
        self._send_to_editor(editor_id, message)

    # === Scene Synchronization ===

    def broadcast_scene_update(self):
        """Broadcast scene state to all editors"""
        for editor_id in self.get_connected_editors():
            self._sync_scene_state(editor_id)

    def _sync_scene_state(self, editor_id: str):
        """Send current scene state to editor"""
        scene_data = self.scene._save_to_dict()

        message = {"event": "scene_state", "scene": scene_data}
        self._send_to_editor(editor_id, message)

    def broadcast_animation_created(self, name: str, clip):
        """Broadcast animation creation event to all editors"""
        message = {"event": "animation_created", "animation": clip.to_dict()}
        self._broadcast(message)

    def push_simulation_state(self):
        """Push current simulation state (called every frame)"""

        entities_state = []
        for entity in self.scene.get_all_entities():
            transform = entity.get_component("transform")
            if transform:
                entities_state.append(
                    {
                        "entity_id": entity.id,
                        "position": transform.position,
                        "rotation": transform.rotation,
                        "scale": transform.scale,
                    }
                )

        if entities_state:
            message = {
                "event": "simulation_update",
                "entities": entities_state,
                "timestamp": time.time(),
            }
            self._broadcast(message)

    # === Command Processing ===

    def process_commands(self):
        """Process pending commands from editors (call in main loop)"""
        processed = 0
        max_per_frame = 10  # Limit commands per frame

        while processed < max_per_frame:
            try:
                command = self._command_queue.get_nowait()
                self._handle_command(command)
                processed += 1
            except queue.Empty:
                break

    def submit_command(self, command: EditorCommand):
        """Submit a command from editor"""
        self._command_queue.put(command)

    def _handle_command(self, command: EditorCommand):
        """Handle a command from editor"""
        cmd_type = command.type
        params = command.params

        try:
            if cmd_type == "request_resource_sync":
                # Editor requests full resource sync
                editor_id = command.editor_id
                self.sync_all_resources(editor_id)

            elif cmd_type == "load_resource":
                # Editor requests to load a resource
                path = params["path"]
                resource_type = ResourceType(params["resource_type"])
                priority = LoadPriority(params.get("priority", LoadPriority.Normal))

                resource_id = self.resource_manager.register_resource(
                    path, resource_type
                )
                success = self.resource_manager.load_resource(resource_id, priority)

                # Notify editor of result
                self._send_to_editor(
                    command.editor_id,
                    {
                        "event": "command_response",
                        "command_type": cmd_type,
                        "success": success,
                        "resource_id": resource_id,
                    },
                )

            elif cmd_type == "create_entity":
                # Create a new entity
                name = params.get("name", "")
                entity = self.scene.create_entity(name)

                self._send_to_editor(
                    command.editor_id,
                    {
                        "event": "command_response",
                        "command_type": cmd_type,
                        "success": True,
                        "entity_id": entity.id,
                    },
                )

                # Broadcast to all editors
                self.broadcast_scene_update()

            elif cmd_type == "destroy_entity":
                # Destroy an entity
                entity_id = params["entity_id"]
                self.scene.destroy_entity(entity_id)

                self._send_to_editor(
                    command.editor_id,
                    {
                        "event": "command_response",
                        "command_type": cmd_type,
                        "success": True,
                    },
                )

                # Broadcast to all editors
                self.broadcast_scene_update()

            elif cmd_type == "modify_component":
                # Modify entity component
                entity_id = params["entity_id"]
                component_type = params["component_type"]
                component_data = params["component_data"]

                print(f"[EditorService] modify_component: entity_id={entity_id}, component_type={component_type}")
                print(f"[EditorService] component_data: {component_data}")

                # Deserialize component based on type
                if component_type == "transform":
                    from .scene import TransformComponent
                    component = TransformComponent(**component_data)
                    print(f"[EditorService] Created TransformComponent: position={component.position}, rotation={component.rotation}, scale={component.scale}")
                elif component_type == "render":
                    from .scene import RenderComponent
                    component = RenderComponent(**component_data)
                else:
                    component = component_data

                # Check if entity exists
                entity = self.scene.get_entity(entity_id)
                if not entity:
                    print(f"[EditorService] Error: Entity {entity_id} not found")
                    raise ValueError(f"Entity {entity_id} not found")

                self.scene.add_component(entity_id, component_type, component)
                print(f"[EditorService] Component added successfully to entity {entity_id}")

                self._send_to_editor(
                    command.editor_id,
                    {
                        "event": "command_response",
                        "command_type": cmd_type,
                        "success": True,
                    },
                )

                # Broadcast scene update to all editors
                print("[EditorService] Broadcasting scene update...")
                self.broadcast_scene_update()
                print("[EditorService] Scene update broadcasted")

            elif cmd_type == "heartbeat":
                # Update editor heartbeat
                with self._editors_lock:
                    if command.editor_id in self._editors:
                        self._editors[command.editor_id].last_heartbeat = time.time()

            else:
                print(f"[EditorService] Unknown command type: {cmd_type}")

        except Exception as e:
            print(f"[EditorService] Error handling command {cmd_type}: {e}")

            # Send error response
            self._send_to_editor(
                command.editor_id,
                {
                    "event": "command_response",
                    "command_type": cmd_type,
                    "success": False,
                    "error": str(e),
                },
            )

    # === Internal Methods ===

    def _service_loop(self):
        """Main service loop (runs in background thread)"""
        last_update = time.time()
        update_interval = 1.0 / self.update_rate_hz

        while self._running:
            try:
                # Check for dead connections
                self._cleanup_dead_connections()

                # Process outgoing message queues
                self._flush_message_queues()

                # Rate limiting
                elapsed = time.time() - last_update
                if elapsed < update_interval:
                    time.sleep(update_interval - elapsed)
                last_update = time.time()

            except Exception as e:
                print(f"[EditorService] Error in service loop: {e}")
                time.sleep(0.1)

    def _cleanup_dead_connections(self):
        """Remove dead editor connections"""
        with self._editors_lock:
            dead_editors = [
                editor_id
                for editor_id, conn in self._editors.items()
                if not conn.is_alive()
            ]

            for editor_id in dead_editors:
                print(f"[EditorService] Editor {editor_id} timed out")
                del self._editors[editor_id]

    def _flush_message_queues(self):
        """Flush pending messages to editors"""
        with self._editors_lock:
            for editor_id, connection in self._editors.items():
                try:
                    while True:
                        message = connection.message_queue.get_nowait()
                        self._send_message_impl(editor_id, message)
                except queue.Empty:
                    pass

    def _send_message_impl(self, editor_id: str, message: dict):
        """
        Actually send a message to an editor

        TODO: Implement actual network communication (WebSocket, TCP, etc.)
        For now, just print the message
        """
        # Placeholder: In production, use WebSocket or TCP socket
        # print(f"[EditorService] -> {editor_id}: {message['event']}")
        pass

    def _broadcast(self, message: dict):
        """Broadcast a message to all connected editors"""
        with self._editors_lock:
            for editor_id in self._editors:
                self._send_to_editor(editor_id, message)

    def _send_to_editor(self, editor_id: str, message: dict):
        """Send a message to a specific editor"""
        with self._editors_lock:
            if editor_id in self._editors:
                self._editors[editor_id].send_message(message)
