"""
Animation data structures for RoboCute

Provides simple keyframe-based animation system for entities.
"""

from typing import List, Dict, Any, Optional
from dataclasses import dataclass, field


@dataclass
class AnimationKeyframe:
    """Single keyframe in an animation sequence"""

    frame: int
    position: List[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])
    rotation: List[float] = field(
        default_factory=lambda: [0.0, 0.0, 0.0, 1.0]
    )  # quaternion (x, y, z, w)
    scale: List[float] = field(default_factory=lambda: [1.0, 1.0, 1.0])

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            "frame": self.frame,
            "position": self.position,
            "rotation": self.rotation,
            "scale": self.scale,
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "AnimationKeyframe":
        """Create from dictionary"""
        return cls(
            frame=data["frame"],
            position=data.get("position", [0.0, 0.0, 0.0]),
            rotation=data.get("rotation", [0.0, 0.0, 0.0, 1.0]),
            scale=data.get("scale", [1.0, 1.0, 1.0]),
        )


@dataclass
class AnimationSequence:
    """Animation sequence for a single entity"""

    entity_id: int
    keyframes: List[AnimationKeyframe] = field(default_factory=list)

    def add_keyframe(self, keyframe: AnimationKeyframe):
        """Add a keyframe to the sequence"""
        self.keyframes.append(keyframe)
        # Keep keyframes sorted by frame
        self.keyframes.sort(key=lambda k: k.frame)

    def get_keyframe_at_frame(self, frame: int) -> Optional[AnimationKeyframe]:
        """Get exact keyframe at specified frame, or None if not found"""
        for kf in self.keyframes:
            if kf.frame == frame:
                return kf
        return None

    def get_total_frames(self) -> int:
        """Get total number of frames in this sequence"""
        if not self.keyframes:
            return 0
        return max(kf.frame for kf in self.keyframes)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            "entity_id": self.entity_id,
            "keyframes": [kf.to_dict() for kf in self.keyframes],
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "AnimationSequence":
        """Create from dictionary"""
        return cls(
            entity_id=data["entity_id"],
            keyframes=[
                AnimationKeyframe.from_dict(kf) for kf in data.get("keyframes", [])
            ],
        )


@dataclass
class AnimationClip:
    """Collection of animation sequences for multiple entities"""

    name: str
    sequences: List[AnimationSequence] = field(default_factory=list)
    fps: float = 30.0
    metadata: Dict[str, Any] = field(default_factory=dict)

    def add_sequence(self, sequence: AnimationSequence):
        """Add an animation sequence to the clip"""
        # Remove existing sequence for same entity if present
        self.sequences = [
            s for s in self.sequences if s.entity_id != sequence.entity_id
        ]
        self.sequences.append(sequence)

    def get_sequence_for_entity(self, entity_id: int) -> Optional[AnimationSequence]:
        """Get animation sequence for specific entity"""
        for seq in self.sequences:
            if seq.entity_id == entity_id:
                return seq
        return None

    def get_total_frames(self) -> int:
        """Get total number of frames in this clip"""
        if not self.sequences:
            return 0
        return max(seq.get_total_frames() for seq in self.sequences)

    def get_duration_seconds(self) -> float:
        """Get duration in seconds"""
        return self.get_total_frames() / self.fps if self.fps > 0 else 0.0

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization"""
        return {
            "name": self.name,
            "sequences": [seq.to_dict() for seq in self.sequences],
            "fps": self.fps,
            "total_frames": self.get_total_frames(),
            "duration_seconds": self.get_duration_seconds(),
            "metadata": self.metadata,
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "AnimationClip":
        """Create from dictionary"""
        return cls(
            name=data["name"],
            sequences=[
                AnimationSequence.from_dict(seq) for seq in data.get("sequences", [])
            ],
            fps=data.get("fps", 30.0),
            metadata=data.get("metadata", {}),
        )
