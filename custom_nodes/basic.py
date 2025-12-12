"""
Basic Builtin Nodes for RoboCute
"""

from typing import Dict, Any, List
import math
import robocute as rbc


@rbc.register_node
class EntityInputNode(rbc.RBCNode):
    """Entity Input Node - Reference scene entities"""

    NODE_TYPE = "entity_input"
    DISPLAY_NAME = "Entity Input"
    CATEGORY = "Scene"
    DESCRIPTION = "Reference an entity from the scene by ID"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="entity_id",
                type="integer",
                required=True,
                default=1,
                description="Entity ID to reference",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="entity",
                type="entity",
                description="Entity reference with ID and name",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        entity_id = int(self.get_input("entity_id", 1))

        print(f"[EntityInputNode] Looking for entity ID: {entity_id}")

        # Validate entity exists in scene context
        if self.context is None:
            print("[EntityInputNode] ✗ No scene context available!")
            raise RuntimeError("EntityInputNode requires a scene context")

        print(
            f"[EntityInputNode] Scene context available, {len(self.context.get_all_entities())} entities in scene"
        )

        entity = self.context.get_entity(entity_id)
        if entity is None:
            print(f"[EntityInputNode] ✗ Entity {entity_id} not found")
            all_entities = self.context.get_all_entities()
            print(
                f"[EntityInputNode] Available entities: {[e.id for e in all_entities]}"
            )
            raise ValueError(f"Entity with ID {entity_id} not found in scene")

        print(f"[EntityInputNode] ✓ Found entity: {entity.name} (ID: {entity.id})")

        # Return entity reference as a dict
        return {"entity": {"id": entity.id, "name": entity.name}}


@rbc.register_node
class EntityGroupInputNode(rbc.RBCNode):
    """Entity Group Input Node - Reference multiple scene entities"""

    NODE_TYPE = "entity_group_input"
    DISPLAY_NAME = "Entity Group Input"
    CATEGORY = "Scene"
    DESCRIPTION = "Reference multiple entities from the scene by IDs"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="entity_ids",
                type="entity_group",
                required=False,
                default="[]",
                description="Entity group - drag and drop entities from SceneHierarchy (Ctrl+click to select multiple)",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="entity_group",
                type="entity_group",
                description="List of entity references with IDs and names",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        entity_ids_input = self.get_input("entity_ids", "[]")

        # Parse entity IDs - can be string (JSON array) or already a list
        entity_ids = []
        if isinstance(entity_ids_input, list):
            # Already a list of entity IDs
            entity_ids = entity_ids_input
        elif isinstance(entity_ids_input, str):
            entity_ids_str = entity_ids_input.strip()

            if not entity_ids_str or entity_ids_str == "" or entity_ids_str == "[]":
                print("[EntityGroupInputNode] No entity IDs provided")
                return {"entity_group": []}

            # Try parsing as JSON array first
            try:
                import json

                parsed = json.loads(entity_ids_str)
                if isinstance(parsed, list):
                    entity_ids = parsed
                else:
                    raise ValueError("Not a list")
            except (json.JSONDecodeError, ValueError):
                # Fall back to comma-separated string
                parts = entity_ids_str.split(",")
                entity_ids = [int(p.strip()) for p in parts if p.strip()]
        else:
            print(
                f"[EntityGroupInputNode] Unexpected input type: {type(entity_ids_input)}"
            )
            return {"entity_group": []}

        if not entity_ids:
            print("[EntityGroupInputNode] No valid entity IDs found")
            return {"entity_group": []}

        print(f"[EntityGroupInputNode] Looking for entity IDs: {entity_ids}")

        # Validate scene context
        if self.context is None:
            print("[EntityGroupInputNode] ✗ No scene context available!")
            raise RuntimeError("EntityGroupInputNode requires a scene context")

        print(
            f"[EntityGroupInputNode] Scene context available, {len(self.context.get_all_entities())} entities in scene"
        )

        # Get all entities
        entity_group = []
        for entity_id in entity_ids:
            entity = self.context.get_entity(entity_id)
            if entity is None:
                print(f"[EntityGroupInputNode] ✗ Entity {entity_id} not found")
                continue

            print(
                f"[EntityGroupInputNode] ✓ Found entity: {entity.name} (ID: {entity.id})"
            )
            entity_group.append({"id": entity.id, "name": entity.name})

        if not entity_group:
            all_entities = self.context.get_all_entities()
            print(
                f"[EntityGroupInputNode] ✗ No entities found. Available entities: {[e.id for e in all_entities]}"
            )
            raise ValueError(f"None of the requested entities found in scene")

        print(f"[EntityGroupInputNode] ✓ Found {len(entity_group)} entity(ies)")

        # Return entity group as a list
        return {"entity_group": entity_group}
