## Command Format
The command format is:
> command_name(arg0, arg1, ...)

If the command's return value is not "None", the command format can be:
> return_value = command_name(arg0: type, arg1: type, ...)
> command_name(return_value)

## Commands (format: "command_name: [arguments...]"):
'display_cam_add_pos': [x: float, y: float, z: float] -> None
'display_cam_rotate': [yaw: float, pitch: float] -> None
'editing_select_object': [uv_x: float, uv_y: float] -> Entity
'editing_select_material': [uv_x: float, uv_y: float] -> MaterialResource
'editing_select_submesh_index': [uv_x: float, uv_y: float] -> int
'entity_transform_set_position': [entity: Entity, x: float, y: float, z: float] -> None
'entity_transform_get_position': [entity: Entity] -> double3
'entity_transform_add_position': [entity: Entity, x: float, y: float, z: float] -> None
'entity_transform_set_rotation': [entity: Entity, x: float, y: float, z: float, w: float] -> None
'entity_transform_get_rotation': [entity: Entity] -> float4
'entity_transform_set_rotation_euler': [entity: Entity, euler_x: float, euler_y: float, euler_z: float] -> None
'entity_transform_set_scale': [entity: Entity, x: float, y: float, z: float] -> None
'entity_transform_get_scale': [entity: Entity] -> double3
'entity_transform_add_scale': [entity: Entity, x: float, y: float, z: float] -> None
'entity_light_add_point_light': [entity: Entity, r: float, g: float, b: float, visible: bool] -> None
'entity_light_add_area_light': [entity: Entity, r: float, g: float, b: float, visible: bool] -> None
'entity_light_add_disk_light': [entity: Entity, r: float, g: float, b: float, visible: bool] -> None
'entity_light_add_spot_light': [entity: Entity, r: float, g: float, b: float, angle_radians: float, small_angle_radians: float, angle_atten_pow: float, visible: bool] -> None
'entity_light_get_luminance': [entity: Entity] -> float3
'entity_light_get_angle_radians': [entity: Entity] -> float
'entity_light_get_small_angle_radians': [entity: Entity] -> float
'entity_light_get_angle_atten_pow': [entity: Entity] -> float
'entity_camera_get_fov': [entity: Entity] -> float
'entity_camera_set_fov': [entity: Entity, value: float] -> None
'entity_camera_get_near_plane': [entity: Entity] -> float
'entity_camera_set_near_plane': [entity: Entity, value: float] -> None
'entity_camera_get_far_plane': [entity: Entity] -> float
'entity_camera_set_far_plane': [entity: Entity, value: float] -> None
'entity_camera_get_focus_distance': [entity: Entity] -> float
'entity_camera_set_focus_distance': [entity: Entity, value: float] -> None
'entity_camera_get_aperture': [entity: Entity] -> float
'entity_camera_set_aperture': [entity: Entity, value: float] -> None
'entity_camera_get_aspect_ratio': [entity: Entity] -> float
'entity_camera_set_aspect_ratio': [entity: Entity, value: float] -> None
'entity_render_get_mat_count': [entity: Entity] -> int
'entity_render_get_mat': [entity: Entity, index: int] -> MaterialResource
'entity_render_remove_object': [entity: Entity] -> None
'entity_add_transform_component': [entity: Entity] -> TransformComponent
'entity_add_render_component': [entity: Entity] -> RenderComponent
'entity_add_light_component': [entity: Entity] -> LightComponent
'entity_add_camera_component': [entity: Entity] -> CameraComponent
'entity_remove_transform_component': [entity: Entity] -> bool
'entity_remove_render_component': [entity: Entity] -> bool
'entity_remove_light_component': [entity: Entity] -> bool
'entity_remove_camera_component': [entity: Entity] -> bool
'resource_material_load_from_openpbr': [mat_res: MaterialResource, openpbr: OpenPBRInterface] -> None
'resource_material_dump_openpbr': [mat_res: MaterialResource] -> OpenPBRInterface