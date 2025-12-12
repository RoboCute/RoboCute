"""
Basic Builtin Nodes for RoboCute
"""

from typing import Dict, Any, List
import math
import robocute as rbc


@rbc.register_node
class PrintNode(rbc.RBCNode):
    """Print Out Given Data Node"""

    NODE_TYPE = "BASIC:OUTPUT"
    DISPLAY_NAME = "Print To Console"
    CATEGORY = "BASIC"
    DESCRIPTION = "Print a given value to console"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="input_string",
                type="string",
                require=False,
                default="",
                description="Input String to Print",
            )
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return []

    def execute(self) -> Dict[str, Any]:
        input_string = str(self.get_input("input_string", ""))
        print(f"[Basic::PrintNode] {input_string}")
        return {}


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

