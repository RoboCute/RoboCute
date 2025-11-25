"""
High-Level Resource Management API

Provides convenient functions and utilities for resource management
in RoboCute, wrapping the lower-level rbc_ext.resource module.
"""

from typing import Optional, List, Callable, Dict, Any
import json
from pathlib import Path

# Re-export from rbc_ext for convenience
try:
    from rbc_ext.resource import (
        ResourceManager,
        ResourceHandle,
        ResourceMetadata,
        ResourceType,
        ResourceState,
        LoadPriority,
        LoadOptions
    )
except ImportError:
    from src.rbc_ext.resource import (
        ResourceManager,
        ResourceHandle,
        ResourceMetadata,
        ResourceType,
        ResourceState,
        LoadPriority,
        LoadOptions
    )

__all__ = [
    'ResourceManager',
    'ResourceHandle',
    'ResourceMetadata',
    'ResourceType',
    'ResourceState',
    'LoadPriority',
    'LoadOptions',
    'create_default_material',
    'load_scene_resources',
    'preload_with_lod',
    'batch_load_resources',
    'manage_memory_budget'
]


# === Convenience Functions ===

def create_default_material(resource_manager: ResourceManager, 
                           name: str = "default",
                           base_color: tuple = (0.8, 0.8, 0.8, 1.0),
                           metallic: float = 0.0,
                           roughness: float = 0.5) -> int:
    """
    Create a default PBR material
    
    Args:
        resource_manager: The resource manager to use
        name: Material name
        base_color: Base color RGBA
        metallic: Metallic value (0-1)
        roughness: Roughness value (0-1)
        
    Returns:
        Material resource ID
    """
    # Create material data
    material_data = {
        "name": name,
        "base_color": list(base_color),
        "metallic": metallic,
        "roughness": roughness,
        "emissive": [0.0, 0.0, 0.0]
    }
    
    # Save to temporary file
    temp_path = Path(f"temp_{name}_material.json")
    with open(temp_path, 'w') as f:
        json.dump(material_data, f)
    
    # Load as resource
    material_id = resource_manager.load_material(
        str(temp_path),
        priority=LoadPriority.High
    )
    
    return material_id


def load_scene_resources(resource_manager: ResourceManager, 
                        scene_file: str,
                        priority: LoadPriority = LoadPriority.High) -> List[int]:
    """
    Batch load all resources from a scene file
    
    Args:
        resource_manager: The resource manager to use
        scene_file: Path to scene JSON file
        priority: Loading priority for all resources
        
    Returns:
        List of resource IDs
    """
    scene_path = Path(scene_file)
    if not scene_path.exists():
        print(f"Scene file not found: {scene_file}")
        return []
    
    with open(scene_file, 'r') as f:
        scene_data = json.load(f)
    
    resource_ids = []
    
    # Batch register resources
    for resource_ref in scene_data.get('resources', []):
        rid = resource_manager.register_resource(
            resource_ref['path'],
            ResourceType(resource_ref['type'])
        )
        resource_ids.append(rid)
    
    # Batch load with specified priority
    for rid in resource_ids:
        resource_manager.load_resource(rid, priority=priority)
    
    print(f"[load_scene_resources] Queued {len(resource_ids)} resources for loading")
    return resource_ids


def preload_with_lod(resource_manager: ResourceManager, 
                    base_path: str,
                    num_levels: int = 4) -> List[int]:
    """
    Preload multiple LOD levels with appropriate priorities
    
    Args:
        resource_manager: The resource manager to use
        base_path: Base path for LOD files (without _lodN suffix)
        num_levels: Number of LOD levels to load
        
    Returns:
        List of resource IDs (ordered by LOD level)
    """
    lod_priorities = [
        LoadPriority.Critical,   # LOD0: Highest priority
        LoadPriority.High,       # LOD1: High priority
        LoadPriority.Normal,     # LOD2: Normal priority
        LoadPriority.Background  # LOD3+: Background
    ]
    
    lod_ids = []
    base_path_obj = Path(base_path)
    stem = base_path_obj.stem
    ext = base_path_obj.suffix
    parent = base_path_obj.parent
    
    for lod_level in range(num_levels):
        # Construct LOD path
        lod_path = parent / f"{stem}_lod{lod_level}{ext}"
        
        # Determine priority
        priority = lod_priorities[min(lod_level, len(lod_priorities) - 1)]
        
        # Register and load
        rid = resource_manager.register_resource(str(lod_path), ResourceType.Mesh)
        
        # Create options with LOD level
        options = LoadOptions(lod_level=lod_level)
        
        resource_manager.load_resource(rid, priority=priority, options=options)
        lod_ids.append(rid)
    
    print(f"[preload_with_lod] Queued {num_levels} LOD levels for {base_path}")
    return lod_ids


def batch_load_resources(resource_manager: ResourceManager,
                        resources: List[Dict[str, Any]],
                        priority: LoadPriority = LoadPriority.Normal,
                        on_complete: Optional[Callable[[int, bool], None]] = None) -> List[int]:
    """
    Batch load multiple resources
    
    Args:
        resource_manager: The resource manager to use
        resources: List of resource dicts with 'path' and 'type' keys
        priority: Loading priority
        on_complete: Optional callback for each resource
        
    Returns:
        List of resource IDs
    """
    resource_ids = []
    
    for res in resources:
        path = res['path']
        res_type = ResourceType(res.get('type', ResourceType.Unknown))
        
        rid = resource_manager.register_resource(path, res_type)
        resource_manager.load_resource(rid, priority=priority, on_complete=on_complete)
        resource_ids.append(rid)
    
    print(f"[batch_load_resources] Queued {len(resource_ids)} resources")
    return resource_ids


def manage_memory_budget(resource_manager: ResourceManager, 
                        entity_count: int,
                        base_budget_mb: int = 1024):
    """
    Dynamically adjust memory budget based on scene complexity
    
    Args:
        resource_manager: The resource manager to use
        entity_count: Number of entities in the scene
        base_budget_mb: Base memory budget in MB
    """
    if entity_count < 1000:
        budget_mb = base_budget_mb
    elif entity_count < 10000:
        budget_mb = base_budget_mb * 2
    else:
        budget_mb = base_budget_mb * 4
    
    resource_manager._cpp_loader.set_cache_budget(budget_mb * 1024 * 1024)
    
    # Periodically clear unused resources
    resource_manager.clear_unused()
    
    print(f"[manage_memory_budget] Set budget to {budget_mb}MB for {entity_count} entities")


# === Resource Utilities ===

class ResourceBatch:
    """
    Helper class for batching resource operations
    """
    
    def __init__(self, resource_manager: ResourceManager):
        self.resource_manager = resource_manager
        self.resources: List[Dict[str, Any]] = []
        
    def add(self, path: str, resource_type: ResourceType, 
            priority: LoadPriority = LoadPriority.Normal):
        """Add a resource to the batch"""
        self.resources.append({
            'path': path,
            'type': resource_type,
            'priority': priority
        })
        
    def load_all(self, on_complete: Optional[Callable[[int, int], None]] = None) -> List[int]:
        """
        Load all resources in the batch
        
        Args:
            on_complete: Optional callback (loaded_count, total_count)
            
        Returns:
            List of resource IDs
        """
        resource_ids = []
        loaded_count = [0]  # Mutable counter
        total = len(self.resources)
        
        def completion_callback(rid: int, success: bool):
            if success:
                loaded_count[0] += 1
                if on_complete:
                    on_complete(loaded_count[0], total)
        
        for res in self.resources:
            rid = self.resource_manager.register_resource(
                res['path'],
                res['type']
            )
            self.resource_manager.load_resource(
                rid,
                priority=res['priority'],
                on_complete=completion_callback
            )
            resource_ids.append(rid)
        
        self.resources.clear()
        return resource_ids


def wait_for_resources(resource_manager: ResourceManager,
                       resource_ids: List[int],
                       timeout: float = 30.0) -> bool:
    """
    Wait for multiple resources to finish loading
    
    Args:
        resource_manager: The resource manager to use
        resource_ids: List of resource IDs to wait for
        timeout: Maximum time to wait in seconds
        
    Returns:
        True if all resources loaded successfully
    """
    import time
    
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        all_loaded = True
        
        for rid in resource_ids:
            if not resource_manager.is_loaded(rid):
                state = resource_manager.get_state(rid)
                if state == ResourceState.Failed:
                    print(f"[wait_for_resources] Resource {rid} failed to load")
                    return False
                all_loaded = False
        
        if all_loaded:
            return True
        
        time.sleep(0.01)
    
        print("[wait_for_resources] Timeout waiting for resources")
    return False
