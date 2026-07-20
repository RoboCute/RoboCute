# RoboCute World API Documentation

This document describes the Python API for interacting with the RoboCute world system.

## Table of Contents

- [RoboCute World API Documentation](#robocute-world-api-documentation)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
    - [Basic Usage](#basic-usage)
  - [App Class](#app-class)
    - [Properties](#properties)
    - [Methods](#methods)
      - [`init(backend_name, project_path, world_path=None, require_render=True)`](#initbackend_name-project_path-world_pathnone-require_rendertrue)
      - [`init_ctx()`](#init_ctx)
      - [`init_world(world_path)`](#init_worldworld_path)
      - [`init_device(backend_name, program_path=BUILTIN_PROGRAM_PATH)`](#init_devicebackend_name-program_pathbuiltin_program_path)
      - [`init_render()`](#init_render)
      - [`init_project(project_path)`](#init_projectproject_path)
      - [`init_display(x=1920, y=1080, display_title="py_window", create_window=True, window_resizable=True, full_screen=False, transparent=False)`](#init_displayx1920-y1080-display_titlepy_window-create_windowtrue-window_resizabletrue-full_screenfalse-transparentfalse)
      - [`init_transparent_display(x=1920, y=1080, offset_x=0, offset_y=0, opacity=0.5, topmost=True, click_through=False, display_title="py_window")`](#init_transparent_displayx1920-y1080-offset_x0-offset_y0-opacity05-topmosttrue-click_throughfalse-display_titlepy_window)
      - [`display_image(dtype=float)`](#display_imagedtypefloat)
      - [`get_display_transform()`](#get_display_transform)
      - [`initialized()`](#initialized)
      - [`set_user_callback(callback)`](#set_user_callbackcallback)
      - [`call_exit()`](#call_exit)
      - [`run(prepare_denoise=False, limit_frame=None)`](#runprepare_denoisefalse-limit_framenone)
      - [`upload_mesh_data(mesh)`](#upload_mesh_datamesh)
      - [`set_ground_plane_mode(mode, scale=100, height=0, material=None)`](#set_ground_plane_modemode-scale100-height0-materialnone)
  - [Core Classes](#core-classes)
    - [RBCContext](#rbccontext)
      - [Methods](#methods-1)
    - [Scene](#scene)
      - [Methods](#methods-2)
    - [Project](#project)
      - [Methods](#methods-3)
    - [Entity](#entity)
      - [Methods](#methods-4)
    - [Component](#component)
      - [Methods](#methods-5)
  - [Components](#components)
    - [TransformComponent](#transformcomponent)
      - [Methods](#methods-6)
    - [RenderComponent](#rendercomponent)
      - [Methods](#methods-7)
    - [CameraComponent](#cameracomponent)
      - [Methods](#methods-8)
    - [LightComponent](#lightcomponent)
      - [Methods](#methods-9)
    - [DataComponent](#datacomponent)
      - [Methods](#methods-10)
    - [AtmosphereComponent](#atmospherecomponent)
      - [Methods](#methods-11)
    - [SkelMeshComponent](#skelmeshcomponent)
      - [Methods](#methods-12)
  - [Resources](#resources)
    - [Resource](#resource)
      - [Methods](#methods-12)
    - [MeshResource](#meshresource)
      - [Methods](#methods-13)
    - [TextureResource](#textureresource)
      - [Methods](#methods-14)
    - [MaterialResource](#materialresource)
      - [Methods](#methods-15)
    - [BufferResource](#bufferresource)
      - [Methods](#methods-16)
    - [SkeletonResource](#skeletonresource)
      - [Methods](#methods-17)
    - [SkinResource](#skinresource)
      - [Methods](#methods-18)
    - [AnimSequenceResource](#animsequenceresource)
      - [Methods](#methods-19)
    - [AnimGraphResource](#animgraphresource)
      - [Methods](#methods-20)
    - [SkelMeshResource](#skelmeshresource)
      - [Methods](#methods-21)
    - [VoxelResource](#voxelresource)
      - [Methods](#methods-22)
    - [SDFVoxelResource](#sdfvoxelresource)
      - [Methods](#methods-23)
  - [Supporting Classes](#supporting-classes)
    - [BasicData](#basicdata)
      - [Methods](#methods-22)
    - [RenderSettings](#rendersettings)
      - [Camera Settings](#camera-settings)
      - [Exposure \& Tonemapping](#exposure--tonemapping)
      - [ACES Tonemapping](#aces-tonemapping)
      - [LPM (Local Photographic Mapping)](#lpm-local-photographic-mapping)
      - [Sun \& Sky](#sun--sky)
      - [Offline Rendering](#offline-rendering)
      - [Post-Processing](#post-processing)
      - [Serialization](#serialization)
    - [EntitiesCollection](#entitiescollection)
      - [Methods](#methods-23)
    - [FileMeta](#filemeta)
      - [Methods](#methods-24)
    - [SelectQuery](#selectquery)
      - [Methods](#methods-25)
    - [BuiltinKernels](#builtinkernels)
      - [Methods](#methods-26)
  - [Enums](#enums)
    - [BasicDataType](#basicdatatype)
    - [ResourceLoadStatus](#resourceloadstatus)
    - [RendererGeometryType](#renderergeometrytype)
    - [BaseObjectType](#baseobjecttype)
    - [DataComponentEventType](#datacomponenteventtype)
    - [TickStage](#tickstage)
    - [LCPixelStorage](#lcpixelstorage)
    - [LCPixelFormat](#lcpixelformat)
  - [Type Aliases](#type-aliases)

---

## Overview

The RoboCute World API provides a Python interface for creating and managing 3D scenes, entities, components, and resources. The API is organized into two layers:

1. **High-level API**: The `App` class provides a simplified interface for common operations
2. **Low-level API**: Direct access to world objects through `robocute.rbc_ext.world`

### Basic Usage

```python
import robocute.rbc_ext as re
from robocute.app import App
from pathlib import Path

# Using the App singleton
app = App()
app.init("cuda", Path("./my_project"))
app.init_display(1920, 1080)
app.run()
```

---

## App Class

The `App` class is a singleton that provides a high-level interface for initializing and running the RoboCute application.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `scene` | `Scene` | The current active scene |
| `ctx` | `RBCContext` | The RBC context instance |
| `display_cam` | `CameraComponent` | The display camera component |
| `last_frame_time` | `float` | Timestamp of the last frame |

### Methods

#### `init(backend_name, project_path, world_path=None, require_render=True)`

Initialize the application with the specified backend and project.

**Parameters:**
- `backend_name` (str): The rendering backend to use (e.g., "cuda", "dx12")
- `project_path` (Path): Path to the project directory
- `world_path` (Path, optional): Path to the world library. Defaults to `project_path/library`
- `require_render` (bool): Whether to initialize rendering

#### `init_ctx()`

Initialize the RBC context.

#### `init_world(world_path)`

Initialize the world system.

**Parameters:**
- `world_path` (Path): Path to the world library

#### `init_device(backend_name, program_path=BUILTIN_PROGRAM_PATH)`

Initialize the rendering device.

**Parameters:**
- `backend_name` (str): The rendering backend
- `program_path` (Path): Path to shader programs

#### `init_render()`

Initialize the rendering system.

#### `init_project(project_path)`

Initialize and load a project.

**Parameters:**
- `project_path` (Path): Path to the project directory

#### `init_display(x=1920, y=1080, display_title="py_window", create_window=True, window_resizable=True, full_screen=False, transparent=False)`

Initialize the display window.

**Parameters:**
- `x` (int): Width of the display
- `y` (int): Height of the display
- `display_title` (str): Window title
- `create_window` (bool): Whether to create a window
- `window_resizable` (bool): Whether the window is resizable
- `full_screen` (bool): Whether to use full screen mode
- `transparent` (bool): Whether the window is transparent

#### `init_transparent_display(x=1920, y=1080, offset_x=0, offset_y=0, opacity=0.5, topmost=True, click_through=False, display_title="py_window")`

Initialize a transparent display window.

**Parameters:**
- `x`, `y` (int): Display dimensions
- `offset_x`, `offset_y` (int): Window position offset
- `opacity` (float): Window opacity (0.0 - 1.0)
- `topmost` (bool): Keep window on top
- `click_through` (bool): Enable click-through
- `display_title` (str): Window title

#### `display_image(dtype=float)`

Get the display image as a Luisa Compute Image2D.

**Returns:** `lc.Image2D` - The display image

#### `get_display_transform()`

Get the transform of the display camera.

**Returns:** `TransformComponent` - The camera's transform

#### `initialized()`

Check if the application has been initialized.

**Returns:** `bool` - True if initialized

#### `set_user_callback(callback)`

Set a callback function to be called every frame.

**Parameters:**
- `callback` (callable): Function to call each frame

#### `call_exit()`

Signal the application to exit on the next frame.

#### `run(prepare_denoise=False, limit_frame=None)`

Run the main application loop.

**Parameters:**
- `prepare_denoise` (bool): Whether to prepare denoising
- `limit_frame` (int, optional): Maximum number of frames to run

#### `upload_mesh_data(mesh)`

Upload mesh data to the GPU.

**Parameters:**
- `mesh` (`MeshResource`): The mesh to upload

#### `set_ground_plane_mode(mode, scale=100, height=0, material=None)`

Configure the ground plane visualization.

**Parameters:**
- `mode` (str): Mode type ('none' to disable, or other values)
- `scale` (float): Plane scale
- `height` (float): Plane height
- `material` (OpenPBRInterface, optional): Custom material for the ground plane

---

## Core Classes

### RBCContext

The main context for managing the RoboCute runtime environment.

#### Methods

| Method | Description |
|--------|-------------|
| `control_camera_add_pos(pos: float3)` | Add position offset to controlled camera |
| `control_camera_add_rotate(yaw, pitch, roll)` | Add rotation to controlled camera |
| `create_display_cam()` | Create a display camera |
| `denoise()` | Apply denoising |
| `destroy_display_cam()` | Destroy the display camera |
| `disable_camera_control()` | Disable camera controls |
| `disable_view()` | Disable view rendering |
| `display_image()` | Get the display image resource |
| `editing_add_click_requires(name, uv)` | Add click requirement for editing |
| `editing_query_click_requires(name)` | Query click result |
| `enable_camera_control()` | Enable camera controls |
| `init_device(rhi_backend, program_path, shader_path)` | Initialize rendering device |
| `init_display(name, size, create_window, window_resizable)` | Initialize display |
| `init_render()` | Initialize rendering |
| `init_transparent_display(name, size, pos, opacity, topmost, click_through)` | Initialize transparent window |
| `init_world(meta_path, binary_path)` | Initialize world system |
| `regist_callback(name, callback)` | Register a callback |
| `reset_view(resolution)` | Reset the view |
| `save_display_image_to(path)` | Save display image to file |
| `should_close()` | Check if window should close |
| `tick(delta_time, tick_stage, prepare_denoise)` | Process one frame tick |
| `unregist_callback(name)` | Unregister a callback |
| `update_skinning_mesh(skinning_mesh, dual_quaternion_buffer)` | Update skinning mesh |
| `upload_mesh_data(mesh)` | Upload mesh data to GPU |
| `upload_texture_data(tex)` | Upload texture data to GPU |

---

### Scene

Represents a scene containing entities.

#### Methods

| Method | Description |
|--------|-------------|
| `add_entity()` | Add a new entity to the scene |
| `get_entities_by_name(name)` | Get all entities with the given name |
| `get_entity(guid)` | Get entity by GUID |
| `get_entity_by_name(name)` | Get first entity with the given name |
| `get_or_add_entity(guid)` | Get existing or create new entity |
| `remove_entity(guid)` | Remove entity by GUID |
| `update_data()` | Update scene data |

---

### Project

Manages project assets and resource importing.

#### Methods

| Method | Description |
|--------|-------------|
| `init(project_root_dir)` | Initialize project with project root directory (must contain rbc_project.json) |
| `scan_project()` | Scan project for resources |
| `import_scene(path, extra_meta)` | Import a scene file |
| `import_texture(path, mip_level, to_vt)` | Import a texture file |
| `import_material(path)` | Import a material |
| `import_mesh(path)` | Import a mesh |
| `import_anim_sequence(path)` | Import an animation sequence |
| `import_skeleton(path)` | Import a skeleton |
| `import_skin(path)` | Import a skin |
| `get_resource(guid, load_content_async)` | Load a resource by GUID |
| `get_file_meta(type_id, dest_path)` | Get file metadata |

---

### Entity

Represents an object in the scene that can have components attached.

#### Methods

| Method | Description |
|--------|-------------|
| `add_component(name)` | Add a component by name |
| `remove_component(name)` | Remove a component by name |
| `get_component(name)` | Get a component by name |
| `set_name(name)` | Set the entity name |
| `name()` | Get the entity name |
| `dispose()` | Dispose of the entity |

---

### Component

Base class for all components that can be attached to entities.

#### Methods

| Method | Description |
|--------|-------------|
| `entity()` | Get the owning entity |
| `update_data()` | Update component data |
| `dispose()` | Dispose of the component |

---

## Components

### TransformComponent

Handles position, rotation, and scale of an entity.

#### Methods

| Method | Description |
|--------|-------------|
| `position()` | Get position as `double3` |
| `rotation()` | Get rotation as `float4` (quaternion) |
| `scale()` | Get scale as `double3` |
| `trs()` | Get TRS matrix as `double4x4` |
| `trs_float()` | Get TRS matrix as `float4x4` |
| `set_pos(pos, recursive)` | Set position |
| `set_rotation(rotation, recursive)` | Set rotation quaternion |
| `set_scale(scale, recursive)` | Set scale |
| `set_trs(pos, rotation, scale, recursive)` | Set all transform properties |
| `set_trs_matrix(trs, recursive)` | Set transform from matrix |
| `children_count()` | Get number of child transforms |
| `remove_children(children)` | Remove child transforms |

---

### RenderComponent

Manages mesh and material rendering for an entity.

#### Methods

| Method | Description |
|--------|-------------|
| `mesh()` | Get the mesh resource |
| `get_material(idx)` | Get material at index |
| `mat_count()` | Get material count |
| `get_tlas_index()` | Get TLAS index |
| `update_mesh(mesh)` | Update the mesh |
| `update_material(mat_vector)` | Update materials |
| `update_object(mat_vector, mesh)` | Update both mesh and materials |
| `remove_object()` | Remove the render object |

---

### CameraComponent

Controls camera settings and rendering.

#### Methods

| Method | Description |
|--------|-------------|
| `enable_camera()` | Enable the camera |
| `disable_camera()` | Disable the camera |
| `fov()` / `set_fov(value)` | Field of view in degrees |
| `aspect_ratio()` / `set_aspect_ratio(value)` | Aspect ratio |
| `auto_aspect_ratio()` / `set_auto_aspect_ratio(value)` | Auto aspect ratio |
| `near_plane()` / `set_near_plane(value)` | Near clipping plane |
| `far_plane()` / `set_far_plane(value)` | Far clipping plane |
| `focus_distance()` / `set_focus_distance(value)` | Focus distance |
| `aperture()` / `set_aperture(value)` | Aperture size |
| `enable_physical_camera()` / `set_enable_physical_camera(value)` | Physical camera mode |
| `set_frame_index(frame_index)` | Set frame index for accumulation |
| `render_settings()` | Get render settings |
| `render_image()` | Get render image resource |
| `config_render_image(size, storage)` | Configure render image |
| `release_render_image()` | Release render image |
| `save_image_to(path)` | Save image to file |
| `set_geometry_export_buffer(buffer, channel_type)` | Set geometry export buffer |
| `clear_geometry_export_buffer()` | Clear geometry export buffer |

---

### LightComponent

Manages light sources.

#### Methods

| Method | Description |
|--------|-------------|
| `add_point_light(luminance, visible)` | Add a point light |
| `add_spot_light(luminance, angle_radians, small_angle_radians, angle_atten_pow, visible)` | Add a spot light |
| `add_area_light(luminance, visible)` | Add an area light |
| `add_disk_light(luminance, visible)` | Add a disk light |
| `luminance()` | Get light luminance |
| `angle_radians()` | Get spot light angle |
| `small_angle_radians()` | Get spot light inner angle |
| `angle_atten_pow()` | Get angle attenuation power |

---

### DataComponent

Stores and manages custom data for entities.

#### Methods

| Method | Description |
|--------|-------------|
| `set_info(name, data)` | Set data value |
| `get_info(name)` | Get data value |
| `has_info(name)` | Check if data exists |
| `remove_info(name)` | Remove data |
| `clear_infos()` | Clear all data |
| `info_count()` | Get number of data entries |
| `count()` | Get number of data entries |
| `get_entity()` | Get the owning entity |
| `bind_event(event_type, callback_name)` | Bind event handler |
| `unbind_event(event_type)` | Unbind event handler |
| `dispose()` | Dispose of the component |

---

### AtmosphereComponent

Manages atmosphere/sky rendering.

#### Methods

| Method | Description |
|--------|-------------|
| `texture()` | Get atmosphere texture |
| `update_texture(tex)` | Update atmosphere texture |

---

### SkelMeshComponent

Component for skeletal mesh rendering.

#### Methods

| Method | Description |
|--------|-------------|
| `get_animation_time()` | Get current animation time |
| `get_bone_transform(bone_index)` | Get transform of a specific bone |
| `get_num_bones()` | Get number of bones |
| `get_playback_speed()` | Get animation playback speed |
| `get_runtime_mesh()` | Get runtime mesh resource |
| `get_skel_mesh_resource()` | Get skeleton mesh resource |
| `set_animation_time(time)` | Set animation time |
| `set_playback_speed(speed)` | Set animation playback speed |
| `set_skel_mesh_resource(res)` | Set skeleton mesh resource |

---

## Resources

### Resource

Base class for all resources.

#### Methods

| Method | Description |
|--------|-------------|
| `install()` | Install the resource |
| `load()` | Load the resource |
| `wait_loading()` | Wait for async loading to complete |
| `load_status()` | Get current load status |
| `path()` | Get resource path |
| `save_to_path()` | Save resource to its path |
| `type()` | Get resource type |
| `get_bool(name)` | Get boolean property |
| `get_int(name)` | Get integer property |
| `get_float(name)` | Get float property |
| `get_string(name)` | Get string property |
| `get_resource(name)` | Get resource property |
| `set_bool(name, value)` | Set boolean property |
| `set_int(name, value)` | Set integer property |
| `set_float(name, value)` | Set float property |
| `set_string(name, value)` | Set string property |
| `set_resource(name, value)` | Set resource property |
| `dispose()` | Dispose of the resource |

---

### MeshResource

Represents a 3D mesh with vertices and triangles.

#### Methods

| Method | Description |
|--------|-------------|
| `create_empty(submesh_offsets, vertex_count, triangle_count, uv_count, contained_normal, contained_tangent)` | Create empty mesh |
| `install()` | Install the mesh resource |
| `create_as_morphing_instance(origin_mesh)` | Create as morphing instance |
| `data_buffer()` | Get raw data buffer |
| `device_data_buffer()` | Get device data buffer |
| `device_mutable_buffer()` | Get device mutable buffer |
| `pos_buffer()` | Get position buffer |
| `normal_buffer()` | Get normal buffer |
| `tangent_buffer()` | Get tangent buffer |
| `triangle_indices_buffer()` | Get triangle indices buffer |
| `uv_buffer(uv_count)` | Get UV buffer |
| `vertex_count()` | Get vertex count |
| `triangle_count()` | Get triangle count |
| `submesh_count()` | Get submesh count |
| `uv_count()` | Get UV layer count |
| `basic_size_bytes()` | Get basic data size |
| `desire_size_bytes()` | Get desired data size |
| `extra_size_bytes()` | Get extra data size |
| `has_data_buffer()` | Check if data buffer exists |
| `contained_normal()` | Check if contains normals |
| `contained_tangent()` | Check if contains tangents |
| `is_transforming_mesh()` | Check if transforming mesh |
| `build_before_tick()` | Build mesh before tick |
| `dispose()` | Dispose of the resource |

---

### TextureResource

Represents a 2D texture.

#### Methods

| Method | Description |
|--------|-------------|
| `create_empty(pixel_storage, size, mip_level, is_virtual_texture)` | Create empty texture |
| `data_buffer()` | Get data buffer |
| `size()` | Get texture size |
| `mip_level()` | Get mip level count |
| `pixel_storage()` | Get pixel storage format |
| `heap_index()` | Get heap index |
| `is_vt()` | Check if virtual texture |
| `has_data_buffer()` | Check if has data buffer |
| `load_executed()` | Check if load was executed |
| `pack_to_tile()` | Pack to tile format |
| `set_skybox()` | Set as skybox |
| `dispose()` | Dispose of the resource |

---

### MaterialResource

Represents a material for rendering.

#### Methods

| Method | Description |
|--------|-------------|
| `load_from_json(json)` | Load material from JSON string |
| `dump_json()` | Dump material to JSON string |
| `mat_code()` | Get material code |
| `dispose()` | Dispose of the resource |

---

### BufferResource

Represents a generic GPU buffer.

#### Methods

| Method | Description |
|--------|-------------|
| `create_empty(size_bytes, create_device_buffer)` | Create empty buffer |
| `buffer()` | Get buffer handle |
| `host_data()` | Get host data pointer |
| `size_bytes()` | Get buffer size |
| `dispose()` | Dispose of the resource |

---

### SkeletonResource

Represents a skeleton for skinned meshes.

#### Methods

| Method | Description |
|--------|-------------|
| `get_joint_names()` | Get joint names |
| `get_joint_parents()` | Get joint parent indices |
| `get_joint_rest_poses()` | Get joint rest poses |
| `get_num_bones()` | Get number of bones |
| `get_num_joints()` | Get number of joints |
| `get_num_soa_joints()` | Get number of SOA joints |
| `get_parent_index(joint_index)` | Get parent index of a joint |
| `ensure_parents_exist()` | Ensure all parent joints exist |
| `ensure_parents_exist_and_sort()` | Ensure parents exist and sort joints |
| `log_brief()` | Log skeleton info |
| `dispose()` | Dispose of the resource |

---

### SkinResource

Represents skinning data for skeletal animation.

#### Methods

| Method | Description |
|--------|-------------|
| `ref_skel()` | Get skeleton reference |
| `ref_mesh()` | Get mesh reference |
| `InverseBindPoses()` | Get inverse bind poses |
| `JointRemaps()` | Get joint remaps |
| `JointRemapsLUT()` | Get joint remaps LUT |
| `generate_LUT()` | Generate lookup table |
| `log_brief()` | Log skin info |
| `dispose()` | Dispose of the resource |

---

### AnimSequenceResource

Resource wrapper for animation sequences.

#### Methods

| Method | Description |
|--------|-------------|
| `get_anim_name()` | Get animation name |
| `set_anim_name(name)` | Set animation name |
| `get_sampling_rate()` | Get sampling rate |
| `set_sampling_rate(rate)` | Set sampling rate |
| `ref_skel()` | Get skeleton reference |
| `log_brief()` | Log resource info |
| `dispose()` | Dispose of the resource |

---

### AnimGraphResource

Represents an animation graph.

#### Methods

| Method | Description |
|--------|-------------|
| `create_simple_anim_graph(anim_seq)` | Create a simple animation graph from sequence |
| `dispose()` | Dispose of the resource |

---

### SkelMeshResource

Represents a skeletal mesh.

#### Methods

| Method | Description |
|--------|-------------|
| `ref_skeleton()` | Get skeleton |
| `ref_skin()` / `GetSkinResource()` | Get skin resource |
| `ref_anim_graph()` | Get animation graph |
| `dispose()` | Dispose of the resource |

---

### VoxelResource

Represents a voxel resource.

#### Methods

| Method | Description |
|--------|-------------|
| `dispose()` | Dispose of the resource |

---

### SDFVoxelResource

Represents a signed-distance field voxel resource.

#### Methods

| Method | Description |
|--------|-------------|
| `dispose()` | Dispose of the resource |

---

## Supporting Classes

### BasicData

Stores primitive data values.

#### Methods

| Method | Description |
|--------|-------------|
| `type()` | Get data type |
| `set_bool(v)` / `get_bool()` | Boolean value |
| `set_int(v)` / `get_int()` | Integer value |
| `set_float(v)` / `get_float()` | Float value |
| `set_string(v)` / `get_string()` | String value |
| `set_resource(res)` / `get_resource()` | Resource reference |

---

### RenderSettings

Configuration for rendering parameters.

#### Camera Settings
- `fov`, `aspect_ratio`, `near_plane`, `far_plane`
- `focus_distance`, `aperture`

#### Exposure & Tonemapping
- `global_exposure`, `gamma`, `max_luminance`, `min_luminance`
- `use_auto_exposure`, `use_hdr_display`, `use_hdr_10`, `use_linear_sdr`

#### ACES Tonemapping
- `aces_contrast`, `aces_saturation`, `aces_temperature`, `aces_tint`
- `aces_lift`, `aces_gain`, `aces_gamma`, `aces_color_filter`
- `aces_hue_shift`, `aces_hdr_paper_white`, `aces_hdr_display_multiplier`
- Color mixer settings for RGB channels

#### LPM (Local Photographic Mapping)
- `lpm_exposure`, `lpm_contrast`, `lpm_saturation`
- `lpm_shoulder`, `lpm_shoulder_contrast`, `lpm_soft_gap`
- `lpm_crosstalk`, `lpm_hdr_max`
- `lpm_display_max_luminance`, `lpm_display_min_luminance`

#### Sun & Sky
- `sun_dir`, `sun_color`, `sun_intensity`, `sun_angle`
- `sky_color`, `sky_angle`, `sky_max_lum`

#### Offline Rendering
- `offline_spp`, `offline_origin_bounce`, `offline_indirect_bounce`

#### Post-Processing
- `denoise`, `chromatic_aberration`
- `distortion_center`, `distortion_intensity`, `distortion_scale`, `distortion_intensity_multiplier`
- `filtering`

#### Serialization
- `serialize_to_json()` - Serialize to JSON
- `deserialize_from_json(json)` - Deserialize from JSON

---

### EntitiesCollection

Collection of entities returned by queries.

#### Methods

| Method | Description |
|--------|-------------|
| `count()` | Get entity count |
| `get_entity(index)` | Get entity at index |

---

### FileMeta

Metadata for project files.

#### Methods

| Method | Description |
|--------|-------------|
| `guid()` | Get file GUID |
| `meta_json()` | Get metadata as JSON |

---

### SelectQuery

Result of a selection query (e.g., mouse picking).

#### Methods

| Method | Description |
|--------|-------------|
| `valid()` | Check if query is valid |
| `prim_id()` | Get primitive ID |
| `barycentric()` | Get barycentric coordinates |
| `get_component()` | Get render component |
| `get_material()` | Get material |
| `get_submesh_index()` | Get submesh index |

---

### BuiltinKernels

Built-in compute kernels for data conversion.

#### Methods

| Method | Description |
|--------|-------------|
| `buffer_to_image(input_buffer, output_image, pixel_offset, pixel_size, swizzle)` | Convert buffer to image |
| `image_to_buffer(input_image, output_buffer, pixel_offset, pixel_size, swizzle)` | Convert image to buffer |

---

## Enums

### BasicDataType

Types for `BasicData`:
- `Bool`, `Int`, `Float`, `String`, `Resource`

### ResourceLoadStatus

Resource loading states:
- `Unloaded`, `Loading`, `Loaded`, `Failed`

### RendererGeometryType

Geometry export types (can be combined with bitwise OR):
- `Depth` - Depth buffer
- `Normal` - Normal vectors
- `ObjectID` - Object ID
- `PrimID` - Primitive ID
- `Barycentric` - Barycentric coordinates
- `Emission` - Emission color
- `Albedo` - Albedo color
- `Position` - Position data
- `Index` - Index data

### BaseObjectType

Object type categories:
- `Object`, `Entity`, `Component`, `Resource`

### DataComponentEventType

Data component event types:
- `OnSet`, `OnGet`, `OnRemove`

### TickStage

Rendering stages:
- `PathTracingPreview`, `PathTracingOffline`, `Rasterization`

### LCPixelStorage

Pixel storage formats (Luisa Compute).

### LCPixelFormat

Pixel formats (Luisa Compute).

---

## Type Aliases

The following vector/matrix types are imported from `robocute.rbc_ext.luisa`:

- `float2`, `float3`, `float4`
- `double2`, `double3`, `double4`
- `uint2`, `uint3`, `uint4`
- `float4x4`, `double4x4`
- `GUID` - Global unique identifier type (from `robocute.rbc_ext._C.rbc_ext_c`)
