"""
Animation Nodes for RoboCute

Provides nodes for creating and managing animations in the scene.
"""

from typing import Dict, Any, List
import math
import robocute as rbc


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
                type="animation_clip",
                description="Generated animation clip",
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

        # Create AnimationClip containing the sequence
        clip = rbc.AnimationClip(name="rotation_animation", fps=fps)
        clip.add_sequence(animation_seq)

        print(
            f"[RotationAnimationNode] ✓ Generated {len(animation_seq.keyframes)} keyframes"
        )
        print(
            f"[RotationAnimationNode]   Total frames: {animation_seq.get_total_frames()}"
        )
        print("[RotationAnimationNode] ✓ Created animation clip with 1 sequence")

        return {"animation": clip}


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
                type="animation_clip",
                required=True,
                description="Animation clip to store (can contain multiple sequences)",
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
        animation_input = self.get_input("animation")
        print(
            f"[AnimationOutputNode] Animation input received: {type(animation_input)}"
        )

        if animation_input is None:
            print("[AnimationOutputNode] ✗ No animation input provided")
            raise ValueError("Animation input is required")

        name = self.get_input("name", "animation")
        fps = float(self.get_input("fps", 30.0))

        print(f"[AnimationOutputNode] Storing animation as '{name}'")

        # Validate scene context
        if self.context is None:
            print("[AnimationOutputNode] ✗ No scene context available!")
            raise RuntimeError("AnimationOutputNode requires a scene context")

        # Handle both AnimationSequence and AnimationClip inputs
        if isinstance(animation_input, rbc.AnimationClip):
            # Already an AnimationClip, just update name and fps
            clip = animation_input
            clip.name = name
            clip.fps = fps
        elif isinstance(animation_input, rbc.AnimationSequence):
            # Single sequence, create clip
            clip = rbc.AnimationClip(name=name, fps=fps)
            clip.add_sequence(animation_input)
        else:
            print(
                f"[AnimationOutputNode] ✗ Unsupported animation input type: {type(animation_input)}"
            )
            raise ValueError(
                f"Unsupported animation input type: {type(animation_input)}"
            )

        # Store in scene
        self.context.scene.add_animation(name, clip)

        total_frames = clip.get_total_frames()
        num_sequences = len(clip.sequences)
        print(
            f"[AnimationOutputNode] Stored animation '{name}' with {num_sequences} sequence(s), {total_frames} frames"
        )

        return {"animation_name": name}


@rbc.register_node
class GroupRotationAnimationNode(rbc.RBCNode):
    """Group Rotation Animation Generator - Create circular rotation animation for multiple entities"""

    NODE_TYPE = "group_rotation_animation"
    DISPLAY_NAME = "Group Rotation Animation"
    CATEGORY = "Animation"
    DESCRIPTION = "Generate rotation animation for multiple entities rotating around center points"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="entity_group",
                type="entity_group",
                required=True,
                description="Group of entities to animate",
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
            rbc.NodeInput(
                name="spacing_angle",
                type="number",
                required=False,
                default=0.0,
                description="Angle spacing between entities in radians (0 = evenly spaced)",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="animation",
                type="animation_clip",
                description="Generated animation clip containing sequences for all entities",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        # Get entity group input
        entity_group = self.get_input("entity_group")
        print(f"[GroupRotationAnimationNode] Entity group input: {entity_group}")

        if (
            not entity_group
            or not isinstance(entity_group, list)
            or len(entity_group) == 0
        ):
            print("[GroupRotationAnimationNode] ✗ No entity group input provided")
            raise ValueError("Entity group input is required and must be non-empty")

        entity_ids = [entity["id"] for entity in entity_group]
        print(
            f"[GroupRotationAnimationNode] Generating rotation animation for {len(entity_ids)} entities: {entity_ids}"
        )

        center_x = float(self.get_input("center_x", 0.0))
        center_y = float(self.get_input("center_y", 0.0))
        center_z = float(self.get_input("center_z", 0.0))

        radius = float(self.get_input("radius", 2.0))

        angular_velocity = float(self.get_input("angular_velocity", 1.0))
        duration_frames = int(self.get_input("duration_frames", 120))
        fps = float(self.get_input("fps", 30.0))

        spacing_angle = float(self.get_input("spacing_angle", 0.0))

        # Calculate angle spacing if not specified (evenly distribute entities)
        if spacing_angle == 0.0 and len(entity_ids) > 1:
            spacing_angle = (2.0 * math.pi) / len(entity_ids)

        # Validate scene context
        if self.context is None:
            print("[GroupRotationAnimationNode] ✗ No scene context available!")
            raise RuntimeError("GroupRotationAnimationNode requires a scene context")

        # Create animation sequences for all entities
        all_sequences = []

        for idx, entity_id in enumerate(entity_ids):
            # Get entity's initial transform if available
            initial_scale = [1.0, 1.0, 1.0]
            initial_offset = [0.0, 0.0, 0.0]

            entity_obj = self.context.get_entity(entity_id)
            if entity_obj:
                transform = entity_obj.get_component("transform")
                if transform:
                    initial_scale = transform.scale
                    initial_offset = transform.position

            # Calculate initial angle offset for this entity
            initial_angle_offset = idx * spacing_angle

            # Generate animation sequence for this entity
            animation_seq = rbc.AnimationSequence(entity_id=entity_id)

            for frame in range(duration_frames + 1):
                # Calculate time in seconds
                time_sec = frame / fps

                # Calculate angle based on angular velocity and initial offset
                angle = angular_velocity * time_sec + initial_angle_offset

                # Calculate position on circular path (rotation in XZ plane)
                x = center_x + radius * math.cos(angle) + initial_offset[0]
                y = center_y + initial_offset[1]
                z = center_z + radius * math.sin(angle) + initial_offset[2]

                # Calculate rotation quaternion (rotate around Y axis)
                half_angle = angle / 2.0
                rotation = [0.0, math.sin(half_angle), 0.0, math.cos(half_angle)]

                # Create keyframe
                keyframe = rbc.AnimationKeyframe(
                    frame=frame,
                    position=[x, y, z],
                    rotation=rotation,
                    scale=initial_scale,
                )

                animation_seq.add_keyframe(keyframe)

            all_sequences.append(animation_seq)
            print(
                f"[GroupRotationAnimationNode] ✓ Generated {len(animation_seq.keyframes)} keyframes for entity {entity_id}"
            )

        # Combine all sequences into a single animation sequence
        # Note: AnimationSequence typically supports one entity, so we need to create
        # a combined sequence or use AnimationClip
        # For now, we'll return the first sequence and note that we need to handle multiple sequences
        # In practice, AnimationOutputNode should handle multiple sequences

        # Create a combined animation clip that contains all sequences
        # Since we're returning animation_sequence type, we'll return the first one
        # and the caller should handle combining them
        # Actually, let's check if AnimationSequence can handle multiple entities

        print(
            f"[GroupRotationAnimationNode] ✓ Generated animations for {len(all_sequences)} entities"
        )
        print(f"[GroupRotationAnimationNode]   Total frames: {duration_frames + 1}")

        # Create AnimationClip containing all sequences

        clip = rbc.AnimationClip(name="group_rotation", fps=fps)
        for seq in all_sequences:
            clip.add_sequence(seq)

        print(
            f"[GroupRotationAnimationNode] ✓ Created animation clip with {len(clip.sequences)} sequences"
        )

        return {"animation": clip}
