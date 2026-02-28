## Command Format
The command format is:
> command_name(arg0, arg1, ...)

If the command's return value is not "None", the command format can be:
> return_value = command_name(arg0, arg1, ...)
> command_name(return_value)

## Commands (format: "command_name: [arguments...]"):
'display_cam_add_pos': [float, float, float] -> None
'display_cam_rotate': [float, float] -> None
'editing_select_object': [float, float] -> Entity
'editing_select_material': [float, float] -> MaterialResource
'editing_select_submesh_index': [float, float] -> int
'entity_transform_set_position': [Entity, float, float, float] -> None
'entity_transform_get_position': [Entity] -> double3
'entity_transform_add_position': [Entity, float, float, float] -> None
'entity_transform_set_rotation': [Entity, float, float, float, float] -> None
'entity_transform_get_rotation': [Entity] -> float4
'entity_transform_set_rotation_euler': [Entity, float, float, float] -> None
'entity_transform_set_scale': [Entity, float, float, float] -> None
'entity_transform_get_scale': [Entity] -> double3
'entity_transform_add_scale': [Entity, float, float, float] -> None
'entity_light_add_point_light': [Entity, float, float, float, bool] -> None
'entity_light_add_area_light': [Entity, float, float, float, bool] -> None
'entity_light_add_disk_light': [Entity, float, float, float, bool] -> None
'entity_light_add_spot_light': [Entity, float, float, float, float, float, float, bool] -> None
'entity_light_get_luminance': [Entity] -> float3
'entity_light_get_angle_radians': [Entity] -> float
'entity_light_get_small_angle_radians': [Entity] -> float
'entity_light_get_angle_atten_pow': [Entity] -> float
'entity_camera_get_fov': [Entity] -> float
'entity_camera_set_fov': [Entity, float] -> None
'entity_camera_get_near_plane': [Entity] -> float
'entity_camera_set_near_plane': [Entity, float] -> None
'entity_camera_get_far_plane': [Entity] -> float
'entity_camera_set_far_plane': [Entity, float] -> None
'entity_camera_get_focus_distance': [Entity] -> float
'entity_camera_set_focus_distance': [Entity, float] -> None
'entity_camera_get_aperture': [Entity] -> float
'entity_camera_set_aperture': [Entity, float] -> None
'entity_camera_get_aspect_ratio': [Entity] -> float
'entity_camera_set_aspect_ratio': [Entity, float] -> None
'entity_render_get_mat_count': [Entity] -> int
'entity_render_get_mat': [Entity, int] -> MaterialResource
'entity_render_remove_object': [Entity] -> None
'entity_add_transform_component': [Entity] -> TransformComponent
'entity_add_render_component': [Entity] -> RenderComponent
'entity_add_light_component': [Entity] -> LightComponent
'entity_add_camera_component': [Entity] -> CameraComponent
'entity_remove_transform_component': [Entity] -> bool
'entity_remove_render_component': [Entity] -> bool
'entity_remove_light_component': [Entity] -> bool
'entity_remove_camera_component': [Entity] -> bool
'resource_material_load_from_openpbr': [MaterialResource, OpenPBRInterface] -> None
'resource_material_dump_openpbr': [MaterialResource] -> OpenPBRInterface