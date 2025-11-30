"""
Scene Context for Node Execution

Provides read-only access to scene data during node execution.
"""

from typing import Optional, List, Any
from .scene import Scene, Entity


class SceneContext:
    """
    Provides scene access during node execution.
    
    Nodes can query scene data but cannot modify it directly.
    This enforces the principle that node execution results should be
    explicit outputs rather than side effects.
    """
    
    def __init__(self, scene: Scene):
        """
        Initialize scene context
        
        Args:
            scene: The scene to provide access to
        """
        self._scene = scene
    
    def get_entity(self, entity_id: int) -> Optional[Entity]:
        """
        Get an entity by ID
        
        Args:
            entity_id: The entity ID
            
        Returns:
            Entity if found, None otherwise
        """
        return self._scene.get_entity(entity_id)
    
    def get_component(self, entity_id: int, component_type: str) -> Optional[Any]:
        """
        Get a component from an entity
        
        Args:
            entity_id: The entity ID
            component_type: Type of component (e.g., 'transform', 'render')
            
        Returns:
            Component if found, None otherwise
        """
        return self._scene.get_component(entity_id, component_type)
    
    def get_all_entities(self) -> List[Entity]:
        """
        Get all entities in the scene
        
        Returns:
            List of all entities
        """
        return self._scene.get_all_entities()
    
    def query_entities_with_component(self, component_type: str) -> List[Entity]:
        """
        Query entities that have a specific component type
        
        Args:
            component_type: Type of component to query for
            
        Returns:
            List of entities with the specified component
        """
        return [
            entity for entity in self._scene.get_all_entities()
            if entity.has_component(component_type)
        ]
    
    def get_scene_name(self) -> str:
        """Get the scene name"""
        return self._scene._name
    
    def get_scene_metadata(self) -> dict:
        """Get the scene metadata"""
        return self._scene.metadata.copy()
    
    @property
    def scene(self) -> Scene:
        """
        Get direct access to the scene for specific operations
        
        Warning: This should be used sparingly. Nodes should generally
        use the query methods above rather than direct scene access.
        """
        return self._scene

