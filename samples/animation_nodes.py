"""
Animation Nodes for RoboCute

Provides nodes for creating and managing animations in the scene.
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
class RotationAnimationNode(rbc.RBCNode):
    """Rotation Animation Generator - Create circular rotation animation"""

    NODE_TYPE = "rotation_animation"
    DISPLAY_NAME = "Rotation Animation"
    CATEGORY = "Animation"
    DESCRIPTION = "Generate animation for entity rotating around a center point"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="entity",
                type="entity",
                required=True,
                description="Entity to animate",
            ),
            rbc.NodeInput(
                name="center_x",
                type="number",
                required=False,
                default=0.0,
                description="Center point X coordinate",
            ),
            rbc.NodeInput(
                name="center_y",
                type="number",
                required=False,
                default=0.0,
                description="Center point Y coordinate",
            ),
            rbc.NodeInput(
                name="center_z",
                type="number",
                required=False,
                default=0.0,
                description="Center point Z coordinate",
            ),
            rbc.NodeInput(
                name="radius",
                type="number",
                required=False,
                default=2.0,
                description="Rotation radius",
            ),
            rbc.NodeInput(
                name="angular_velocity",
                type="number",
                required=False,
                default=1.0,
                description="Angular velocity in radians/second",
            ),
            rbc.NodeInput(
                name="duration_frames",
                type="integer",
                required=False,
                default=120,
                description="Total duration in frames",
            ),
            rbc.NodeInput(
                name="fps",
                type="number",
                required=False,
                default=30.0,
                description="Frames per second",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="animation",
                type="animation_sequence",
                description="Generated animation sequence",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        # Get inputs
        entity = self.get_input("entity")
        print(f"[RotationAnimationNode] Entity input: {entity}")

        if entity is None:
            print("[RotationAnimationNode] ✗ No entity input provided")
            raise ValueError("Entity input is required")

        entity_id = entity["id"]
        print(
            f"[RotationAnimationNode] Generating rotation animation for entity {entity_id}"
        )

        center_x = float(self.get_input("center_x", 0.0))
        center_y = float(self.get_input("center_y", 0.0))
        center_z = float(self.get_input("center_z", 0.0))
        radius = float(self.get_input("radius", 10.0))
        angular_velocity = float(self.get_input("angular_velocity", 5.0))
        duration_frames = int(self.get_input("duration_frames", 120))
        fps = float(self.get_input("fps", 30.0))

        # Get entity's initial transform if available
        initial_position = [center_x + radius, center_y, center_z]
        initial_rotation = [0.0, 0.0, 0.0, 1.0]  # Identity quaternion
        initial_scale = [1.0, 1.0, 1.0]

        if self.context:
            entity_obj = self.context.get_entity(entity_id)
            if entity_obj:
                transform = entity_obj.get_component("transform")
                if transform:
                    initial_scale = transform.scale

        # Generate animation keyframes
        animation_seq = rbc.AnimationSequence(entity_id=entity_id)

        for frame in range(duration_frames + 1):
            # Calculate time in seconds
            time_sec = frame / fps

            # Calculate angle based on angular velocity
            angle = angular_velocity * time_sec

            # Calculate position on circular path (rotation in XZ plane)
            x = center_x + radius * math.cos(angle)
            y = center_y
            z = center_z + radius * math.sin(angle)

            # Calculate rotation quaternion (rotate around Y axis)
            # Quaternion for rotation around Y axis: [0, sin(angle/2), 0, cos(angle/2)]
            half_angle = angle / 2.0
            rotation = [0.0, math.sin(half_angle), 0.0, math.cos(half_angle)]

            # Create keyframe
            keyframe = rbc.AnimationKeyframe(
                frame=frame, position=[x, y, z], rotation=rotation, scale=initial_scale
            )

            animation_seq.add_keyframe(keyframe)

        print(
            f"[RotationAnimationNode] ✓ Generated {len(animation_seq.keyframes)} keyframes"
        )
        print(
            f"[RotationAnimationNode]   Total frames: {animation_seq.get_total_frames()}"
        )

        return {"animation": animation_seq}


@rbc.register_node
class AnimationOutputNode(rbc.RBCNode):
    """Animation Output Node - Store animation in scene"""

    NODE_TYPE = "animation_output"
    DISPLAY_NAME = "Animation Output"
    CATEGORY = "Animation"
    DESCRIPTION = "Store generated animation in the scene"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="animation",
                type="animation_sequence",
                required=True,
                description="Animation sequence to store",
            ),
            rbc.NodeInput(
                name="name",
                type="string",
                required=True,
                default="animation",
                description="Name for the animation",
            ),
            rbc.NodeInput(
                name="fps",
                type="number",
                required=False,
                default=30.0,
                description="Frames per second",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="animation_name",
                type="string",
                description="Name of the stored animation",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        animation_seq = self.get_input("animation")
        print(f"[AnimationOutputNode] Animation input received: {type(animation_seq)}")

        if animation_seq is None:
            print("[AnimationOutputNode] ✗ No animation input provided")
            raise ValueError("Animation input is required")

        name = self.get_input("name", "animation")
        fps = float(self.get_input("fps", 30.0))

        print(f"[AnimationOutputNode] Storing animation as '{name}'")

        # Validate scene context
        if self.context is None:
            print("[AnimationOutputNode] ✗ No scene context available!")
            raise RuntimeError("AnimationOutputNode requires a scene context")

        # Create animation clip
        clip = rbc.AnimationClip(name=name, fps=fps)
        clip.add_sequence(animation_seq)

        # Store in scene
        self.context.scene.add_animation(name, clip)

        print(
            f"[AnimationOutputNode] Stored animation '{name}' with {animation_seq.get_total_frames()} frames"
        )

        return {"animation_name": name}
