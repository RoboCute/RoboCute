"""Scene module for managing scene data with pydantic models."""

from pydantic import BaseModel
from typing import Optional, List, Any, Dict
from .camera_controller import CameraController


class Model(BaseModel):
    """Model configuration with transform and resource references.
    
    Attributes:
        position: 3D position as (x, y, z) tuple
        rotation: Quaternion rotation as (x, y, z, w) tuple
        scale: 3D scale as (x, y, z) tuple
        mesh_name: Name of the mesh resource
        materials_name: List of material resource names
    """
    position: tuple[float, float, float] = (0.0, 0.0, 0.0)
    rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)
    mesh_name: str = ""
    materials_name: List[str] = []


class Scene:
    """Scene class managing models and camera controller.
    
    This is a Python-level scene wrapper that provides:
    - Pydantic-based Model type for scene objects
    - Camera controller for view management
    - Reserved space for future members
    
    Attributes:
        project: Reference to the project this scene belongs to
        models: List of Model instances in the scene
        camera_controller: Camera controller for this scene
    """
    
    def __init__(self, project: Optional[Any] = None):
        """Initialize a Scene.
        
        Args:
            project: Optional project reference this scene belongs to
        """
        self.project = project
        self.models: List[Model] = []
        self.camera_controller = CameraController()
        # Reserved for future members
        self._reserved: Dict[str, Any] = {}
    
    def add_model(self, model: Model) -> None:
        """Add a model to the scene.
        
        Args:
            model: Model instance to add
        """
        self.models.append(model)
    
    def remove_model(self, index: int) -> None:
        """Remove a model from the scene by index.
        
        Args:
            index: Index of the model to remove
        """
        if 0 <= index < len(self.models):
            del self.models[index]
    
    def get_model(self, index: int) -> Optional[Model]:
        """Get a model by index.
        
        Args:
            index: Index of the model to get
            
        Returns:
            Model instance or None if index is out of range
        """
        if 0 <= index < len(self.models):
            return self.models[index]
        return None
    
    def clear_models(self) -> None:
        """Remove all models from the scene."""
        self.models.clear()
    
    def __len__(self) -> int:
        """Return the number of models in the scene."""
        return len(self.models)
    
    def __repr__(self) -> str:
        return f"Scene(models={len(self.models)}, project={self.project is not None})"
