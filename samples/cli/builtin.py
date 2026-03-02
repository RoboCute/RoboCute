import cli.executor as executor
import cli.rbc_app as app
import mat_builtin as mat
# You can use "src/rbc_meta/function_analyzer.py samples\mat_builtin.py" to generate api table


def rbc_app_register(cli_table: executor.CLITable):
    cli_table.add_function('display_cam_add_pos', app.display_cam_add_pos)
    cli_table.add_function('display_cam_rotate', app.display_cam_rotate)
    cli_table.add_function('editing_select_object', app.editing_select_object)
    cli_table.add_function('editing_select_material', app.editing_select_material)
    cli_table.add_function('editing_select_submesh_index', app.editing_select_submesh_index)
    cli_table.add_function('entity_transform_set_position',
                           app.entity_transform_set_position)
    cli_table.add_function('entity_transform_get_position',
                           app.entity_transform_get_position)
    cli_table.add_function('entity_transform_add_position',
                           app.entity_transform_add_position)
    cli_table.add_function('entity_transform_set_rotation',
                           app.entity_transform_set_rotation)
    cli_table.add_function('entity_transform_get_rotation',
                           app.entity_transform_get_rotation)
    cli_table.add_function('entity_transform_set_rotation_euler',
                           app.entity_transform_set_rotation_euler)
    cli_table.add_function('entity_transform_set_scale',
                           app.entity_transform_set_scale)
    cli_table.add_function('entity_transform_get_scale',
                           app.entity_transform_get_scale)
    cli_table.add_function('entity_transform_add_scale',
                           app.entity_transform_add_scale)
    cli_table.add_function('entity_light_add_point_light',
                           app.entity_light_add_point_light)
    cli_table.add_function('entity_light_add_area_light',
                           app.entity_light_add_area_light)
    cli_table.add_function('entity_light_add_disk_light',
                           app.entity_light_add_disk_light)
    cli_table.add_function('entity_light_add_spot_light',
                           app.entity_light_add_spot_light)
    cli_table.add_function('entity_light_get_luminance',
                           app.entity_light_get_luminance)
    cli_table.add_function('entity_light_get_angle_radians',
                           app.entity_light_get_angle_radians)
    cli_table.add_function('entity_light_get_small_angle_radians',
                           app.entity_light_get_small_angle_radians)
    cli_table.add_function('entity_light_get_angle_atten_pow',
                           app.entity_light_get_angle_atten_pow)
    cli_table.add_function('entity_camera_get_fov', app.entity_camera_get_fov)
    cli_table.add_function('entity_camera_set_fov', app.entity_camera_set_fov)
    cli_table.add_function('entity_camera_get_near_plane',
                           app.entity_camera_get_near_plane)
    cli_table.add_function('entity_camera_set_near_plane',
                           app.entity_camera_set_near_plane)
    cli_table.add_function('entity_camera_get_far_plane',
                           app.entity_camera_get_far_plane)
    cli_table.add_function('entity_camera_set_far_plane',
                           app.entity_camera_set_far_plane)
    cli_table.add_function('entity_camera_get_focus_distance',
                           app.entity_camera_get_focus_distance)
    cli_table.add_function('entity_camera_set_focus_distance',
                           app.entity_camera_set_focus_distance)
    cli_table.add_function('entity_camera_get_aperture',
                           app.entity_camera_get_aperture)
    cli_table.add_function('entity_camera_set_aperture',
                           app.entity_camera_set_aperture)
    cli_table.add_function('entity_camera_get_aspect_ratio',
                           app.entity_camera_get_aspect_ratio)
    cli_table.add_function('entity_camera_set_aspect_ratio',
                           app.entity_camera_set_aspect_ratio)
    cli_table.add_function('entity_render_get_mat_count',
                           app.entity_render_get_mat_count)
    cli_table.add_function('entity_render_get_mat', app.entity_render_get_mat)
    cli_table.add_function('entity_render_remove_object',
                           app.entity_render_remove_object)
    cli_table.add_function('entity_add_transform_component',
                           app.entity_add_transform_component)
    cli_table.add_function('entity_add_render_component',
                           app.entity_add_render_component)
    cli_table.add_function('entity_add_light_component',
                           app.entity_add_light_component)
    cli_table.add_function('entity_add_camera_component',
                           app.entity_add_camera_component)
    cli_table.add_function('entity_remove_transform_component',
                           app.entity_remove_transform_component)
    cli_table.add_function('entity_remove_render_component',
                           app.entity_remove_render_component)
    cli_table.add_function('entity_remove_light_component',
                           app.entity_remove_light_component)
    cli_table.add_function('entity_remove_camera_component',
                           app.entity_remove_camera_component)
    cli_table.add_function('resource_material_load_from_openpbr',
                           app.resource_material_load_from_openpbr)
    cli_table.add_function('resource_material_dump_openpbr',
                           app.resource_material_dump_openpbr)
