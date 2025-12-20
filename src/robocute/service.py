"""
Service Base Classes and Server Management

Provides a dependency injection framework for Editor and NodeGraph services.
"""

from abc import ABC, abstractmethod
from typing import Optional, List
import threading
import uvicorn
from fastapi import FastAPI, APIRouter


class Service(ABC):
    """
    Base class for all services that can be registered with the Server.
    
    Services provide functionality that can be injected into the server,
    allowing for modular and testable architecture.
    """
    
    def __init__(self, name: str):
        """
        Initialize service
        
        Args:
            name: Service name for identification
        """
        self.name = name
        self._app: Optional[FastAPI] = None
    
    @abstractmethod
    def register_routes(self, app: FastAPI) -> None:
        """
        Register service routes with the FastAPI application.
        
        This method is called by the Server when the service is registered.
        Services should register their endpoints here.
        
        Args:
            app: The FastAPI application instance
        """
        pass
    
    def get_router(self) -> Optional[APIRouter]:
        """
        Optional: Return an APIRouter instead of registering routes directly.
        
        If a service returns a router, it will be included in the app.
        Otherwise, register_routes will be called.
        
        Returns:
            APIRouter instance or None
        """
        return None
    
    def on_start(self) -> None:
        """
        Called when the server starts.
        
        Override this method to perform initialization tasks.
        """
        pass
    
    def on_stop(self) -> None:
        """
        Called when the server stops.
        
        Override this method to perform cleanup tasks.
        """
        pass
    
    def set_app(self, app: FastAPI) -> None:
        """
        Set the FastAPI application reference.
        
        Args:
            app: The FastAPI application instance
        """
        self._app = app
    
    @property
    def app(self) -> Optional[FastAPI]:
        """Get the FastAPI application reference"""
        return self._app


class Server:
    """
    Server manages FastAPI application and service registration.
    
    Provides dependency injection for services like EditorService and NodeGraphService.
    """
    
    def __init__(self, title: str = "RoboCute Server", version: str = "0.1.0"):
        """
        Initialize server
        
        Args:
            title: Server title
            version: Server version
        """
        self.title = title
        self.version = version
        self._app = FastAPI(title=title, version=version)
        self._services: List[Service] = []
        self._running = False
        self._http_server_thread: Optional[threading.Thread] = None
        self.port = 5555
        self.host = "127.0.0.1"
    
    def register_service(self, service: Service) -> None:
        """
        Register a service with the server.
        
        The service's routes will be registered with the FastAPI app.
        
        Args:
            service: Service instance to register
        """
        if service in self._services:
            print(f"[Server] Service '{service.name}' already registered")
            return
        
        # Set app reference
        service.set_app(self._app)
        
        # Register routes
        router = service.get_router()
        if router:
            self._app.include_router(router)
            print(f"[Server] Registered service '{service.name}' via router")
        else:
            service.register_routes(self._app)
            print(f"[Server] Registered service '{service.name}' via register_routes")
        
        self._services.append(service)
    
    def unregister_service(self, service: Service) -> None:
        """
        Unregister a service from the server.
        
        Args:
            service: Service instance to unregister
        """
        if service in self._services:
            service.on_stop()
            self._services.remove(service)
            print(f"[Server] Unregistered service '{service.name}'")
    
    def get_service(self, name: str) -> Optional[Service]:
        """
        Get a registered service by name.
        
        Args:
            name: Service name
            
        Returns:
            Service instance or None if not found
        """
        for service in self._services:
            if service.name == name:
                return service
        return None
    
    @property
    def app(self) -> FastAPI:
        """Get the FastAPI application"""
        return self._app
    
    def start(self, port: int = 5555, host: str = "127.0.0.1") -> None:
        """
        Start the server and all registered services.
        
        Args:
            port: HTTP server port
            host: HTTP server host
        """
        if self._running:
            return
        
        self.port = port
        self.host = host
        
        # Start all services
        self._running = True
        for service in self._services:
            try:
                service.on_start()
            except Exception as e:
                print(f"[Server] Error starting service '{service.name}': {e}")
        
        # Start HTTP server in separate thread
        self._http_server_thread = threading.Thread(
            target=self._run_http_server, name="HTTPServer", daemon=True
        )
        self._http_server_thread.start()
        
        print(f"[Server] Started on {host}:{port}")
    
    def _run_http_server(self):
        """Run the HTTP server"""
        config = uvicorn.Config(
            self._app,
            host=self.host,
            port=self.port,
            log_level="info",
            access_log=False,
        )
        server = uvicorn.Server(config)
        server.run()
    
    def stop(self) -> None:
        """Stop all registered services and HTTP server"""
        if not self._running:
            return
        
        self._running = False
        
        # Stop all services
        for service in self._services:
            try:
                service.on_stop()
            except Exception as e:
                print(f"[Server] Error stopping service '{service.name}': {e}")
        
        # HTTP server thread will stop automatically when main thread exits
        print("[Server] Stopped")
    
    def is_running(self) -> bool:
        """Check if server is running"""
        return self._running

