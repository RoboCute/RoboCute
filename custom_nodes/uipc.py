from typing import Dict, Any, List
import math
import robocute as rbc


@rbc.register_node
class UIPCSimNode(rbc.RBCNode):
    """
    UIPCSimNode
    - input: a list of entities selected to simulate, will be later query
    - configure:
        - start frame
        - end frame
    - ouput: an animation clip
    """

    NODE_TYPE = "uipc_sim"
    DISPLAY_NAME = "UIPC Simulation"
    CATEGORY = "Physics"
    DESCRIPTION = "Simulate a subscene with pyuipc"

    @classmethod
    def get_inputs(cls) -> List[rbc.NodeInput]:
        return [
            rbc.NodeInput(
                name="entity_group",
                type="entity_group",
                required=True,
                description="Group of entities to simulate",
            ),
            rbc.NodeInput(
                name="duration_frames",
                type="integer",
                required=False,
                default=120,
                description="Total duration in frames",
            ),
            rbc.NodeInput(
                name="dt",
                type="number",
                required=False,
                default=0.01,
                description="Delta Time in seconds of physics for each frame",
            ),
            rbc.NodeInput(
                name="fps",
                type="number",
                required=False,
                default=30.0,
                description="Frames Per Seconds to Play Animation",
            ),
        ]

    @classmethod
    def get_outputs(cls) -> List[rbc.NodeOutput]:
        return [
            rbc.NodeOutput(
                name="animation",
                type="animation_clip",
                description="Generate animation clip containing sequences for all entities",
            )
        ]

    def execute(self) -> Dict[str, Any]:
        entity_group = self.get_input("entity_group")
        print(f"[{self.DISPLAY_NAME}] Entity group input: {entity_group}")

        if (
            not entity_group
            or not isinstance(entity_group, list)
            or len(entity_group) == 0
        ):
            print(f"[{self.DISPLAY_NAME}] ✗ No entity group input provided")
            raise ValueError("Entity group input is required and must be non-empty")

        entity_ids = [entity["id"] for entity in entity_group]
        fps = float(self.get_input("fps", 30.0))
        print(
            f"[{self.DISPLAY_NAME}] Generating rotation animation for {len(entity_ids)} entities: {entity_ids}"
        )
        duration_frames = int(self.get_input("duration_frames", 120))
        dt = float(self.get_input("dt"), 0.01)

        # Create animation sequences for all entities
        all_sequences = []

        # TODO: do uipc sim here

        # Dispatch Result
        for idx, entity_id in enumerate(entity_ids):
            # Generate animation sequence for this entity
            animation_seq = rbc.AnimationSequence(entity_id=entity_id)
            for frame in range(duration_frames + 1):
                keyframe = rbc.AnimationKeyframe(
                    frame=frame,
                    position=[1.0, 1.0, 1.0],
                    rotation=[0.0, 0.0, 0.0, 1.0],
                    scale=[1.0, 1.0, 1.0],
                )

                animation_seq.add_keyframe(keyframe)

            all_sequences.append(animation_seq)

        clip = rbc.AnimationClip(name="uipc_physics", fps=fps)
        for seq in all_sequences:
            clip.add_sequence(seq)
        print(
            f"[{self.DISPLAY_NAME}] ✓ Created animation clip with {len(clip.sequences)} sequences"
        )
        return {"animation": clip}
