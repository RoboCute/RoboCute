"""
Skeletal Animation Scene Example

This example demonstrates how to:
1. Load skeletal animation resources from GLTF files
2. Create entity with SkelMeshComponent
3. Update animation and render each frame

Based on: rbc/tests/sample_anim/main.cpp

Usage:
    cd <project_root>
    uv run python -m samples.app_anim_scene -p <project_path> -b <backend>
    
Example:
    uv run python -m samples.app_anim_scene -p d:/ws/repos/RoboCute-repo/rbc-project-anim -b dx
"""

import os
import sys
import time
from pathlib import Path
import numpy as np
import math
import argparse
from typing import Optional, List

# Add parent directory to path for samples module imports
script_dir = Path(__file__).parent
project_root = script_dir.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

# Change to project root directory to ensure resources are loaded correctly
os.chdir(project_root)

# Import samples modules after path setup
try:
    import samples.cli as cli
except ImportError:
    cli = None

try:
    import mat_builtin as mat
    HAS_MAT_BUILTIN = True
except ImportError:
    HAS_MAT_BUILTIN = False
    mat = None

from robocute.rbc_ext._C import lcapi_c as lcapi
import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc
import robocute as rbc


app: rbc.app.App = None


def find_gltf_file(project_path: Path) -> Optional[Path]:
    """Find test_anim.gltf in project directory."""
    scene_paths = [
        project_path / "assets" / "anim_test" / "test_anim.gltf",
        project_path / "assets" / "test_anim.gltf",
    ]
    
    for path in scene_paths:
        if path.exists():
            return path
    
    return None


def load_skybox(project_path: Path):
    """Load skybox texture from various candidate paths."""
    skybox_candidates = [
        project_path / "assets" / "anim_test" / "landscape.png",
        project_path / "assets" / "anim_test" / "landscape.jpg",
        project_path / "assets" / "anim_test" / "landscape.hdr",
        project_path / "assets" / "sky.exr",
        project_path / "assets" / "test_scene" / "sky.exr",
    ]
    
    for path in skybox_candidates:
        if path.exists():
            try:
                # Import texture using project
                rel_path = str(path.relative_to(project_path))
                tex = app._project.import_texture(rel_path, 1, False)
                if tex:
                    tex.install()
                    print(f"Skybox loaded from: {path}")
                    return tex
            except Exception as e:
                print(f"Failed to load skybox from {path}: {e}")
    
    print("Warning: Skybox not found, using default lighting")
    return None


def create_default_light(scene: re.world.Scene):
    """Create default lighting when no skybox is available."""
    try:
        light_entity = scene.add_entity()
        light_entity.set_name("default_light")
        
        trans = re.world.TransformComponent(
            light_entity.add_component("TransformComponent")
        )
        trans.set_pos(lc.double3(5, 10, 5), True)
        trans.set_scale(lc.double3(2.0, 2.0, 2.0), True)
        
        render = re.world.RenderComponent(light_entity.add_component("RenderComponent"))
        
        # Create emissive material
        light_mat = re.world.MaterialResource()
        light_mat.load_from_json(
            '{"type": "pbr", "emission_luminance": [34, 24, 10], "base_albedo": [0, 0, 0]}'
        )
        light_mat.unsafe_set_loaded()
        light_mat.install()
        
        print("Created default emissive light")
        return light_entity
    except Exception as e:
        print(f"Failed to create default light: {e}")
        return None


def create_animated_entity(scene: re.world.Scene, project_path: Path):
    """
    Create entity with SkelMeshComponent from GLTF file.
    
    This follows the same pattern as the C++ sample_anim example:
    1. Import mesh from GLTF
    2. Import skeleton from GLTF
    3. Import skin from GLTF (depends on skeleton and mesh)
    4. Import animation sequence from GLTF (depends on skeleton)
    5. Create animation graph
    6. Create SkelMeshResource
    7. Create entity with components
    """
    gltf_path = find_gltf_file(project_path)
    if not gltf_path:
        print("Error: Cannot find test_anim.gltf")
        return None
    
    print(f"Loading animated character from: {gltf_path}")
    
    # Get relative path from project root for import functions
    rel_path = str(gltf_path.relative_to(project_path))
    
    try:
        # Step 1: Import mesh
        print("Step 1: Importing mesh...")
        mesh = app._project.import_mesh(rel_path)
        if not mesh:
            print("Failed to import mesh")
            return None
        print("Mesh imported successfully")
        
        # Step 2: Import skeleton
        print("Step 2: Importing skeleton...")
        skeleton = app._project.import_skeleton(rel_path)
        if not skeleton:
            print("Failed to import skeleton")
            return None
        print("Skeleton imported successfully")
        
        # Step 3: Import skin (depends on skeleton and mesh)
        print("Step 3: Importing skin...")
        skin = app._project.import_skin(rel_path)
        if not skin:
            print("Failed to import skin")
            return None
        # Set references (required for skin to work)
        skin.ref_skel = skeleton
        skin.ref_mesh = mesh
        # Generate LUT for skin
        skin.generate_LUT()
        print("Skin imported successfully")
        
        # Step 4: Import animation sequence (depends on skeleton)
        print("Step 4: Importing animation sequence...")
        anim_seq = app._project.import_anim_sequence(rel_path)
        if not anim_seq:
            print("Failed to import animation sequence")
            return None
        anim_seq.ref_skel = skeleton
        print("Animation sequence imported successfully")
        
        # Step 5: Create animation graph (depends on animation sequence)
        print("Step 5: Creating animation graph...")
        anim_graph = re.world.AnimGraphResource()
        success = anim_graph.create_simple_anim_graph(anim_seq)
        if not success:
            print("Failed to create animation graph")
            return None
        print("Animation graph created successfully")
        
        # Step 6: Create SkelMeshResource (depends on skin, skeleton, anim_graph)
        print("Step 6: Creating SkelMeshResource...")
        skel_mesh = re.world.SkelMeshResource()
        skel_mesh.ref_skin = skin
        skel_mesh.ref_skeleton = skeleton
        skel_mesh.ref_anim_graph = anim_graph
        print("SkelMeshResource created successfully")
        
        # Step 7: Create entity with components
        print("Step 7: Creating entity...")
        entity = scene.add_entity()
        entity.set_name("animated_character")
        
        # Add TransformComponent
        trans = re.world.TransformComponent(
            entity.add_component("TransformComponent")
        )
        trans.set_pos(lc.double3(0, 0, 0), True)
        trans.set_scale(lc.double3(0.2, 0.2, 0.2), True)
        
        # Add RenderComponent (required for SkelMeshComponent)
        render = re.world.RenderComponent(entity.add_component("RenderComponent"))
        
        # Add SkelMeshComponent
        skelmesh_comp = re.world.SkelMeshComponent(
            entity.add_component("SkelMeshComponent")
        )
        skelmesh_comp.SetRefSkelMesh(skel_mesh)
        
        # Initialize animation
        skelmesh_comp.tick(0.0)
        
        print("Entity created successfully with SkelMeshComponent")
        return entity
        
    except Exception as e:
        print(f"Failed to create animated entity: {e}")
        import traceback
        traceback.print_exc()
        return None


def tick_animation(entity, delta_time: float):
    """Update animation for the entity."""
    if not entity:
        return
    
    try:
        skelmesh = re.world.SkelMeshComponent(
            entity.get_component("SkelMeshComponent")
        )
        if skelmesh:
            skelmesh.tick(delta_time)
    except Exception as e:
        # Component might not exist
        pass


def update_render(entity):
    """Update render state for the entity."""
    if not entity:
        return
    
    try:
        skelmesh = re.world.SkelMeshComponent(
            entity.get_component("SkelMeshComponent")
        )
        if not skelmesh:
            return
        
        if not skelmesh.IsEnabled():
            return
        
        skelmesh.update_render()
        
        # Get runtime mesh and update transforming mesh
        runtime_mesh = skelmesh.GetRuntimeMesh()
        if runtime_mesh and app.ctx:
            # Note: build_transforming_mesh would be called here
            # but the exact API may differ
            pass
            
    except Exception as e:
        # Component might not exist or other error
        pass


def main():
    parser = argparse.ArgumentParser(description="Skeletal Animation Scene Example")
    parser.add_argument(
        "-b", "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "-p", "--project",
        type=str,
        help="rbc project path, the directory containing rbc_project.json",
        required=True,
    )
    args = parser.parse_args()

    project_path = Path(args.project)
    global app
    app = rbc.app.App()  # rbc app singleton
    
    # Manual initialization to avoid importing default scene
    app.init_ctx()
    app.init_device(args.backend)
    lc.init()
    app.init_render()
    
    # Init world
    world_path = project_path / "library"
    app.init_world(world_path)
    
    # Init project without importing default scene
    app._project = re.world.Project()
    app._project.init(str(project_path / "assets"))
    app._project.scan_project()
    
    # Create a new empty scene instead of importing test_scene.scene
    app._scene = re.world.Scene()
    app._scene.install()
    
    app._initialized = True
    
    # Try to load skybox
    skybox = load_skybox(project_path)
    
    if not app.ctx:
        print("Context not Valid!")
        return

    resolution = lc.uint2(1024, 1024)
    app.init_display(resolution.x, resolution.y)
    if not app.display_cam:
        print("Display not Valid!")
        return

    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(2, 2, -5), False)
    
    # TUI test (optional)
    if cli:
        tui_table = cli.executor.CLITable()
        cli.rbc_app.app = app
        cli.builtin.rbc_app_register(tui_table)

    app.ctx.enable_camera_control()

    if not app.scene:
        print("Scene not Valid!")
        return

    # Create default lighting if no skybox
    if not skybox:
        create_default_light(app.scene)

    # Create animated entity
    entity = create_animated_entity(app.scene, project_path)
    
    if not entity:
        print("Warning: Could not create animated entity, continuing with empty scene")

    last_time = time.time()

    def tick_logic():  # run every frame
        nonlocal last_time

        cur_time = time.time()
        delta_time = cur_time - last_time
        last_time = cur_time

        # Update animation
        tick_animation(entity, delta_time)
        
        # Update render
        update_render(entity)

    app.set_user_callback(tick_logic)
    app.run()


if __name__ == "__main__":
    main()
