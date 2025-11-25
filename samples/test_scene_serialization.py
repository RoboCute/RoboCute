"""
Test script to check scene serialization format
"""

import robocute as rbc
import json

def main():
    print("=== Testing Scene Serialization ===\n")
    
    # Create scene
    scene = rbc.Scene(cache_budget_mb=2048)
    
    # Create entity
    robot = scene.create_entity("Robot")
    print(f"Created entity: {robot.name} (ID: {robot.id})")
    
    # Add transform component
    transform = rbc.TransformComponent(
        position=[0.0, 1.0, 3.0],
        rotation=[0.0, 0.0, 0.0, 1.0],
        scale=[1.0, 1.0, 1.0],
    )
    scene.add_component(robot.id, "transform", transform)
    print(f"Added transform component")
    
    # Add render component
    render = rbc.RenderComponent(
        mesh_id=123,
        material_ids=[],
    )
    scene.add_component(robot.id, "render", render)
    print(f"Added render component: mesh_id={render.mesh_id}")
    
    # Check entity components
    print(f"\nEntity components: {list(robot.components.keys())}")
    print(f"Has render component: {'render' in robot.components}")
    
    if 'render' in robot.components:
        rc = robot.components['render']
        print(f"Render component type: {type(rc)}")
        print(f"Render component __dict__: {rc.__dict__}")
    
    # Serialize scene
    scene_data = scene._save_to_dict()
    
    # Print JSON
    print("\n=== Serialized Scene JSON ===")
    print(json.dumps(scene_data, indent=2))
    
    # Check entities in serialized data
    print("\n=== Entities in Serialized Data ===")
    for entity_data in scene_data.get('entities', []):
        print(f"Entity {entity_data['id']}: {entity_data['name']}")
        print(f"  Components: {list(entity_data['components'].keys())}")
        for comp_type, comp_data in entity_data['components'].items():
            print(f"    {comp_type}: {comp_data}")

if __name__ == "__main__":
    main()

