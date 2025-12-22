"""
RoboCute Scene Management

Provides a high-level interface for managing scenes with integrated
resource management, ECS, and editor synchronization.
"""

from typing import Optional, Dict, List, Any
from dataclasses import dataclass
import json
from pathlib import Path

from rbc_ext.resource import ResourceManager, ResourceType, LoadPriority

from .animation import AnimationClip


@dataclass
class TransformComponent:
    """Transform component for entities"""

    position: List[float] = None
    rotation: List[float] = None  # Quaternion (x, y, z, w)
    scale: List[float] = None

    def __post_init__(self):
        if self.position is None:
            self.position = [0.0, 0.0, 0.0]
        if self.rotation is None:
            self.rotation = [0.0, 0.0, 0.0, 1.0]
        if self.scale is None:
            self.scale = [1.0, 1.0, 1.0]


@dataclass
class RenderComponent:
    """Render component with mesh and material references"""

    mesh_id: int = 0
    material_ids: List[int] = None

    def __post_init__(self):
        if self.material_ids is None:
            self.material_ids = []


@dataclass
class Entity:
    """Entity with components"""

    id: int
    name: str
    components: Dict[str, Any]

    def __init__(self, id: int, name: str = ""):
        self.id = id
        self.name = name or f"Entity_{id}"
        self.components = {}

    def add_component(self, component_type: str, component: Any):
        """Add a component to this entity"""
        self.components[component_type] = component

    def get_component(self, component_type: str) -> Optional[Any]:
        """Get a component from this entity"""
        return self.components.get(component_type)

    def has_component(self, component_type: str) -> bool:
        """Check if entity has a component"""
        return component_type in self.components


class Scene:
    """
    Runtime Scene data collection

    Manages entities, components, and resources for a scene.
    Can be loaded/saved as a .rbc file.
    """

    def __init__(self, cache_budget_mb: int = 2048):
        """Initialize scene with resource manager"""
        self._entities: Dict[int, Entity] = {}
        self._next_entity_id: int = 1
        self._running: bool = False
        self._name: str = "Untitled"

        # Initialize resource manager
        self.resource_manager = ResourceManager(cache_budget_mb=cache_budget_mb)

        # Scene metadata
        self.metadata: Dict[str, Any] = {}

        # Animation storage
        self._animations: Dict[str, AnimationClip] = {}

    def start(self):
        """Start the scene (initializes resource manager)"""
        if not self._running:
            self.resource_manager.start()
            self._running = True

    def stop(self):
        """Stop the scene"""
        if self._running:
            self.resource_manager.stop()
            self._running = False

    def is_running(self) -> bool:
        """Check if scene is running"""
        return self._running

    def update(self, dt: float):
        """Update scene (called every frame)"""
        # TODO: Update systems
        pass

    # === Entity Management ===

    def create_entity(self, name: str = "") -> Entity:
        """Create a new entity"""
        entity_id = self._next_entity_id
        self._next_entity_id += 1

        entity = Entity(entity_id, name)
        self._entities[entity_id] = entity

        return entity

    def destroy_entity(self, entity_id: int):
        """Destroy an entity"""
        if entity_id in self._entities:
            del self._entities[entity_id]

    def get_entity(self, entity_id: int) -> Optional[Entity]:
        """Get an entity by ID"""
        return self._entities.get(entity_id)

    def get_all_entities(self) -> List[Entity]:
        """Get all entities in the scene"""
        return list(self._entities.values())

    # === Component Management ===

    def add_component(self, entity_id: int, component_type: str, component: Any):
        """Add a component to an entity"""
        entity = self.get_entity(entity_id)
        if entity:
            entity.add_component(component_type, component)

    def get_component(self, entity_id: int, component_type: str) -> Optional[Any]:
        """Get a component from an entity"""
        entity = self.get_entity(entity_id)
        return entity.get_component(component_type) if entity else None

    # === Animation Management ===

    def add_animation(self, name: str, clip: AnimationClip):
        """Add an animation clip to the scene"""
        self._animations[name] = clip

    def get_animation(self, name: str) -> Optional[AnimationClip]:
        """Get an animation clip by name"""
        return self._animations.get(name)

    def get_all_animations(self) -> Dict[str, AnimationClip]:
        """Get all animation clips in the scene"""
        return self._animations.copy()

    def remove_animation(self, name: str):
        """Remove an animation clip from the scene"""
        if name in self._animations:
            del self._animations[name]

    # === Scene Serialization ===

    def load(self, path: str):
        """Load scene from file"""
        if not path.endswith(".rbc"):
            print("Invalid Scene Path, use '.rbc' file format")
            return

        path_obj = Path(path)
        if not path_obj.exists():
            print(f"Scene file not found: {path}")
            return

        with open(path, "r") as f:
            data = json.load(f)

        self._load_from_dict(data)
        print(f"Scene loaded from {path}")

    def save(self, path: str):
        """Save scene to file"""
        if not path.endswith(".rbc"):
            path += ".rbc"

        data = self._save_to_dict()

        with open(path, "w") as f:
            json.dump(data, f, indent=2)

        print(f"Scene saved to {path}")

    def _load_from_dict(self, data: dict):
        """Load scene from dictionary"""
        self._name = data.get("name", "Untitled")
        self.metadata = data.get("metadata", {})

        # Load resources
        resources = data.get("resources", [])
        for res in resources:
            resource_id = self.resource_manager.register_resource(
                res["path"], ResourceType(res["type"])
            )
            # Optionally load immediately
            if res.get("preload", False):
                self.resource_manager.load_resource(resource_id)

        # Load entities
        entities_data = data.get("entities", [])
        for entity_data in entities_data:
            entity = self.create_entity(entity_data.get("name", ""))

            # Load components
            for comp_type, comp_data in entity_data.get("components", {}).items():
                if comp_type == "transform":
                    component = TransformComponent(**comp_data)
                elif comp_type == "render":
                    component = RenderComponent(**comp_data)
                else:
                    component = comp_data
                entity.add_component(comp_type, component)

        # Load animations
        animations_data = data.get("animations", {})
        for name, clip_data in animations_data.items():
            clip = AnimationClip.from_dict(clip_data)
            self._animations[name] = clip

    def _save_to_dict(self) -> dict:
        """Save scene to dictionary"""
        data = {
            "name": self._name,
            "metadata": self.metadata,
            "resources": [],
            "entities": [],
            "animations": {},
        }

        # Save resources (collect from all entities)
        resource_ids = set()
        for entity in self._entities.values():
            render_comp = entity.get_component("render")
            if render_comp:
                if render_comp.mesh_id:
                    resource_ids.add(render_comp.mesh_id)
                resource_ids.update(render_comp.material_ids)

        for rid in resource_ids:
            metadata = self.resource_manager.get_metadata(rid)
            if metadata:
                data["resources"].append(
                    {
                        "id": metadata.id,
                        "type": metadata.type,
                        "path": metadata.path,
                        "preload": True,
                    }
                )

        # Save entities
        for entity in self._entities.values():
            entity_data = {"id": entity.id, "name": entity.name, "components": {}}

            for comp_type, component in entity.components.items():
                if hasattr(component, "__dict__"):
                    entity_data["components"][comp_type] = component.__dict__
                else:
                    entity_data["components"][comp_type] = component

            data["entities"].append(entity_data)

        # Save animations
        for name, clip in self._animations.items():
            data["animations"][name] = clip.to_dict()

        return data

    # === Resource Convenience Methods ===

    def load_mesh(self, path: str, priority: LoadPriority = LoadPriority.Normal) -> int:
        """Load a mesh resource"""
        return self.resource_manager.load_mesh(path, priority)

    def load_texture(
        self, path: str, priority: LoadPriority = LoadPriority.Normal
    ) -> int:
        """Load a texture resource"""
        return self.resource_manager.load_texture(path, priority)

    def load_material(
        self, path: str, priority: LoadPriority = LoadPriority.Normal
    ) -> int:
        """Load a material resource"""
        return self.resource_manager.load_material(path, priority)
