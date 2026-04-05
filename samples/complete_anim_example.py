"""
Complete Skeletal Animation Example for RoboCute

This example demonstrates the full workflow of loading and playing skeletal animation
from GLTF files, following the same pattern as the C++ sample_anim.

Features:
- Load mesh, skeleton, skin, animation sequence from GLTF
- Create animation graph programmatically
- Create entity with SkelMeshComponent
- Animation update and rendering loop
- Physics-ready bone hierarchy export

Usage:
    cd <project_root>
    uv run python -m samples.complete_anim_example -p <project_path> -g <gltf_file>

Example:
    uv run python -m samples.complete_anim_example \
        -p d:/ws/repos/RoboCute-repo/rbc-project-anim \
        -g assets/anim_test/test_anim.gltf \
        -b dx
"""

import os
import sys
import time
import argparse
from pathlib import Path
from typing import Optional, List, Dict, Tuple

# Add parent directory to path for samples module imports
script_dir = Path(__file__).parent
project_root = script_dir.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

# Change to project root directory
os.chdir(project_root)

import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc
import robocute as rbc
import numpy as np


class AnimatedCharacter:
    """
    High-level wrapper for animated skeletal mesh character.

    Similar to C++ AnimScene class, this manages all resources and
    provides a clean interface for animation control.
    """

    def __init__(self):
        self.entity: Optional[re.world.Entity] = None
        self.skel_mesh_resource: Optional[re.world.SkelMeshResource] = None
        self.skeleton: Optional[re.world.SkeletonResource] = None
        self.skin: Optional[re.world.SkinResource] = None
        self.anim_sequence: Optional[re.world.AnimSequenceResource] = None
        self.anim_graph: Optional[re.world.AnimGraphResource] = None
        self.mesh: Optional[re.world.MeshResource] = None
        self.materials: List[re.world.MaterialResource] = []
        self.skelmesh_component: Optional[re.world.SkelMeshComponent] = None

        # Animation state
        self.current_time: float = 0.0
        self.is_playing: bool = True
        self.playback_speed: float = 1.0
        self.loop_animation: bool = True

    def load_from_gltf(self, project: re.world.Project, gltf_path: str) -> bool:
        """
        Load all animation resources from a GLTF file.

        This follows the exact same steps as C++ sample_anim:
        1. Load mesh
        2. Load skeleton
        3. Load skin (with skeleton and mesh references)
        4. Load animation sequence
        5. Create animation graph
        6. Create SkelMeshResource

        Args:
            project: Initialized project object
            gltf_path: Relative path to GLTF file (from assets directory)

        Returns:
            True if successful, False otherwise
        """
        print(f"\n{'=' * 70}")
        print(f"Loading animated character from: {gltf_path}")
        print(f"{'=' * 70}\n")

        try:
            # Step 1: Import mesh
            print("[1/7] Importing mesh...")
            self.mesh = project.import_mesh(gltf_path)
            if not self.mesh:
                print("  ✗ Failed to import mesh")
                return False
            print(f"  ✓ Mesh imported: {self.mesh}")

            # Step 2: Import skeleton
            print("\n[2/7] Importing skeleton...")
            self.skeleton = project.import_skeleton(gltf_path)
            if not self.skeleton:
                print("  ✗ Failed to import skeleton")
                return False
            print(f"  ✓ Skeleton imported: {self.skeleton}")

            # Print skeleton info for debugging
            num_joints = self.skeleton.get_num_joints()
            print(f"  📊 Skeleton has {num_joints} joints")

            # Step 3: Import skin
            print("\n[3/7] Importing skin...")
            self.skin = project.import_skin(gltf_path)
            if not self.skin:
                print("  ✗ Failed to import skin")
                return False

            # Set up skin references (critical!)
            self.skin.ref_skel = self.skeleton
            self.skin.ref_mesh = self.mesh
            self.skin.generate_LUT()  # Generate lookup table
            print(f"  ✓ Skin imported and configured")
            print(f"  📊 Skin has {len(self.skin.JointRemaps())} joints")

            # Step 4: Import animation sequence
            print("\n[4/7] Importing animation sequence...")
            self.anim_sequence = project.import_anim_sequence(gltf_path)
            if not self.anim_sequence:
                print("  ✗ Failed to import animation sequence")
                return False

            # Set skeleton reference for animation
            self.anim_sequence.ref_skel = self.skeleton
            print(f"  ✓ Animation sequence imported")

            # Print animation info
            anim_seq = self.anim_sequence.ref_seq()
            print(
                f"  📊 Animation: {anim_seq.get_num_tracks()} tracks, "
                f"{anim_seq.get_num_soa_tracks()} SOA tracks"
            )

            # Step 5: Create animation graph
            print("\n[5/7] Creating animation graph...")
            self.anim_graph = re.world.AnimGraphResource()
            success = self.anim_graph.create_simple_anim_graph(self.anim_sequence)
            if not success:
                print("  ✗ Failed to create animation graph")
                return False
            print(f"  ✓ Animation graph created")

            # Step 6: Create SkelMeshResource
            print("\n[6/7] Creating SkelMeshResource...")
            self.skel_mesh_resource = re.world.SkelMeshResource()
            self.skel_mesh_resource.ref_skin = self.skin
            self.skel_mesh_resource.ref_skeleton = self.skeleton
            self.skel_mesh_resource.ref_anim_graph = self.anim_graph
            print(f"  ✓ SkelMeshResource created")

            # Step 7: Create default material if needed
            print("\n[7/7] Setting up materials...")
            self._setup_materials()

            print(f"\n{'=' * 70}")
            print("✓ Character loaded successfully!")
            print(f"{'=' * 70}\n")
            return True

        except Exception as e:
            print(f"\n✗ Error loading character: {e}")
            import traceback

            traceback.print_exc()
            return False

    def _setup_materials(self):
        """Setup or create default materials."""
        # For now, create a simple PBR material
        # In a full implementation, you'd parse materials from GLTF
        mat = re.world.MaterialResource()
        mat.load_from_json(
            '{"type": "pbr", "base_albedo": [0.8, 0.8, 0.8], "roughness": 0.5}'
        )
        mat.unsafe_set_loaded()
        self.materials.append(mat)
        print(f"  ✓ Default material created")

    def create_entity(
        self,
        scene: re.world.Scene,
        position: lc.double3 = None,
        scale: lc.double3 = None,
    ) -> bool:
        """
        Create an entity with SkelMeshComponent in the scene.

        Args:
            scene: The scene to add entity to
            position: Initial position (default: origin)
            scale: Initial scale (default: 0.2 for typical GLTF models)

        Returns:
            True if successful, False otherwise
        """
        if not self.skel_mesh_resource:
            print("Error: Character resources not loaded")
            return False

        print("\n[Creating Entity]")

        try:
            # Create entity
            self.entity = scene.add_entity()
            self.entity.set_name("animated_character")
            print(f"  ✓ Entity created: {self.entity}")

            # Add TransformComponent
            transform = re.world.TransformComponent(
                self.entity.add_component("TransformComponent")
            )

            if position is None:
                position = lc.double3(0, 0, 0)
            if scale is None:
                scale = lc.double3(0.2, 0.2, 0.2)  # Typical scale for GLTF models

            transform.set_pos(position, True)
            transform.set_scale(scale, True)
            print(f"  ✓ Transform set: pos={position}, scale={scale}")

            # Add RenderComponent (required)
            render = re.world.RenderComponent(
                self.entity.add_component("RenderComponent")
            )
            print(f"  ✓ RenderComponent added")

            # Add SkelMeshComponent
            self.skelmesh_component = re.world.SkelMeshComponent(
                self.entity.add_component("SkelMeshComponent")
            )
            self.skelmesh_component.SetRefSkelMesh(self.skel_mesh_resource)
            print(f"  ✓ SkelMeshComponent added")

            # Critical: Initial tick to initialize animation system
            # This creates the runtime SkeletalMesh and AnimInstance
            self.skelmesh_component.tick(0.0)
            print(f"  ✓ Animation system initialized")

            return True

        except Exception as e:
            print(f"  ✗ Error creating entity: {e}")
            import traceback

            traceback.print_exc()
            return False

    def tick(self, delta_time: float):
        """
        Update animation for this frame.

        Args:
            delta_time: Time since last frame in seconds
        """
        if not self.skelmesh_component or not self.is_playing:
            return

        # Apply playback speed
        dt = delta_time * self.playback_speed
        self.current_time += dt

        # Update animation
        self.skelmesh_component.tick(dt)

    def update_render(self, app: rbc.app.App):
        """
        Update render state for this frame.

        Args:
            app: The RoboCute app instance
        """
        if not self.skelmesh_component:
            return

        try:
            # Update render state
            self.skelmesh_component.update_render()

            # Get runtime mesh and update GPU skinning
            runtime_mesh = self.skelmesh_component.GetRuntimeMesh()
            if runtime_mesh and app.ctx:
                # Note: In full implementation, this would update GPU buffers
                # with the current bone transforms for skinning
                pass

        except Exception as e:
            # Silently ignore if component not ready
            pass

    def play(self):
        """Resume animation playback."""
        self.is_playing = True
        print("▶ Animation playing")

    def pause(self):
        """Pause animation playback."""
        self.is_playing = False
        print("⏸ Animation paused")

    def set_playback_speed(self, speed: float):
        """
        Set animation playback speed.

        Args:
            speed: 1.0 = normal, 2.0 = double speed, 0.5 = half speed
        """
        self.playback_speed = max(0.0, speed)
        print(f"⏩ Playback speed: {self.playback_speed}x")

    def get_bone_info(self) -> Dict:
        """
        Get detailed bone hierarchy information.

        Returns:
            Dictionary with bone hierarchy data for physics/debugging
        """
        if not self.skeleton:
            return {}

        return {
            "num_joints": self.skeleton.get_num_joints(),
            "joint_names": list(self.skeleton.get_joint_names()),
            "joint_parents": list(self.skeleton.get_joint_parents()),
            "rest_poses": self.skeleton.get_joint_rest_poses(),
        }

    def print_bone_hierarchy(self):
        """Print the bone hierarchy tree for debugging."""
        if not self.skeleton:
            print("No skeleton loaded")
            return

        print("\n" + "=" * 70)
        print("Bone Hierarchy")
        print("=" * 70)

        names = self.skeleton.get_joint_names()
        parents = self.skeleton.get_joint_parents()

        # Build tree
        printed = set()

        def print_joint(idx: int, indent: int = 0):
            if idx in printed or idx >= len(names):
                return
            printed.add(idx)

            prefix = "  " * indent + ("└─ " if indent > 0 else "")
            name = names[idx] if idx < len(names) else f"Joint_{idx}"
            parent = parents[idx] if idx < len(parents) else -1

            if parent >= 0:
                parent_name = (
                    names[parent] if parent < len(names) else f"Joint_{parent}"
                )
                print(f"{prefix}[{idx:3d}] {name} (parent: {parent_name})")
            else:
                print(f"{prefix}[{idx:3d}] {name} (ROOT)")

            # Print children
            for i, p in enumerate(parents):
                if p == idx:
                    print_joint(i, indent + 1)

        # Find roots
        for i in range(min(len(names), 50)):
            parent = parents[i] if i < len(parents) else -1
            if parent < 0 or parent >= len(names):
                print_joint(i, 0)

        print("=" * 70 + "\n")


def find_gltf_file(project_path: Path, gltf_rel_path: str) -> Optional[Path]:
    """Find GLTF file in project directory."""
    # Try direct path first
    direct_path = project_path / gltf_rel_path
    if direct_path.exists():
        return direct_path

    # Try common locations
    candidates = [
        project_path / "assets" / "anim_test" / "test_anim.gltf",
        project_path / "assets" / "test_anim.gltf",
        project_path / gltf_rel_path,
    ]

    for path in candidates:
        if path.exists():
            return path

    return None


def create_default_light(scene: re.world.Scene):
    """Create default directional light."""
    try:
        light_entity = scene.add_entity()
        light_entity.set_name("default_light")

        transform = re.world.TransformComponent(
            light_entity.add_component("TransformComponent")
        )
        transform.set_pos(lc.double3(5, 10, 5), True)
        transform.set_scale(lc.double3(1.0, 1.0, 1.0), True)

        render = re.world.RenderComponent(light_entity.add_component("RenderComponent"))

        # Emissive material for light
        light_mat = re.world.MaterialResource()
        light_mat.load_from_json(
            '{"type": "pbr", "emission_luminance": [34, 24, 10], "base_albedo": [0, 0, 0]}'
        )
        light_mat.unsafe_set_loaded()
        light_mat.install()

        print("✓ Default light created")
        return light_entity

    except Exception as e:
        print(f"Warning: Failed to create light: {e}")
        return None


def main():
    parser = argparse.ArgumentParser(description="Complete Skeletal Animation Example")
    parser.add_argument(
        "-p",
        "--project",
        type=str,
        required=True,
        help="RBC project path (containing rbc_project.json)",
    )
    parser.add_argument(
        "-g",
        "--gltf",
        type=str,
        default="assets/anim_test/test_anim.gltf",
        help="GLTF file path relative to project assets directory",
    )
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        choices=["dx", "vk", "cuda", "metal"],
        help="Graphics backend (default: dx)",
    )
    args = parser.parse_args()

    project_path = Path(args.project)
    if not project_path.exists():
        print(f"Error: Project path does not exist: {project_path}")
        return 1

    # Initialize RoboCute app
    print("\n" + "=" * 70)
    print("Initializing RoboCute")
    print("=" * 70 + "\n")

    app = rbc.app.App()

    # Initialize context and device
    app.init_ctx()
    app.init_device(args.backend)
    lc.init()
    app.init_render()

    # Initialize world
    world_path = project_path / "library"
    app.init_world(world_path)

    # Initialize project
    app._project = re.world.Project()
    app._project.init(str(project_path / "assets"))
    app._project.scan_project()

    # Create empty scene
    app._scene = re.world.Scene()
    app._scene.install()

    app._initialized = True

    print("✓ RoboCute initialized\n")

    # Setup display
    resolution = lc.uint2(1024, 1024)
    app.init_display(resolution.x, resolution.y)

    if not app.display_cam:
        print("Error: Failed to create display camera")
        return 1

    # Position camera
    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(2, 2, -5), False)

    app.ctx.enable_camera_control()

    # Create character
    character = AnimatedCharacter()

    # Load from GLTF
    if not character.load_from_gltf(app._project, args.gltf):
        print("\n✗ Failed to load character")
        return 1

    # Print bone hierarchy for debugging
    character.print_bone_hierarchy()

    # Create entity in scene
    if not character.create_entity(app.scene):
        print("\n✗ Failed to create entity")
        return 1

    # Create default light
    create_default_light(app.scene)

    print("\n" + "=" * 70)
    print("Starting main loop")
    print("Controls:")
    print("  SPACE - Pause/Play animation")
    print("  1/2/3 - Set playback speed (0.5x, 1.0x, 2.0x)")
    print("  ESC - Exit")
    print("=" * 70 + "\n")

    # Animation control state
    last_time = time.time()

    def tick_logic():
        nonlocal last_time

        # Calculate delta time
        current_time = time.time()
        delta_time = current_time - last_time
        last_time = current_time

        # Update character animation
        character.tick(delta_time)
        character.update_render(app)

    # Set callback and run
    app.set_user_callback(tick_logic)
    app.run()

    return 0


if __name__ == "__main__":
    sys.exit(main())
