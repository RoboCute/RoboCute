
# This File is Generated From Python Def
# Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
# ================== GENERATED CODE BEGIN ==================
from rbc_ext._C.test_py_codegen import *




class Object:
    def __init__(self, handle):
        self._handle = handle


    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class Entity:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__Entity__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def add_component(self, name: str):
        return Entity__add_component__(self._handle, name)
    def get_component(self, name: str):
        return Entity__get_component__(self._handle, name)
    def remove_component(self, name: str):
        return Entity__remove_component__(self._handle, name)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class Component:
    def __init__(self, handle):
        self._handle = handle


    def entity(self):
        return Entity(Component__entity__(self._handle))
    def update_data(self):
        Component__update_data__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class TransformComponent:
    def __init__(self, handle):
        self._handle = handle


    def children_count(self):
        return TransformComponent__children_count__(self._handle)
    def position(self):
        return TransformComponent__position__(self._handle)
    def remove_children(self, children):
        return TransformComponent__remove_children__(self._handle, children)
    def rotation(self):
        return TransformComponent__rotation__(self._handle)
    def scale(self):
        return TransformComponent__scale__(self._handle)
    def set_pos(self, pos: double3, recursive: bool):
        TransformComponent__set_pos__(self._handle, pos, recursive)
    def set_rotation(self, rotation: float4, recursive: bool):
        TransformComponent__set_rotation__(self._handle, rotation, recursive)
    def set_scale(self, scale: double3, recursive: bool):
        TransformComponent__set_scale__(self._handle, scale, recursive)
    def set_trs(self, pos: double3, rotation: float4, scale: double3, recursive: bool):
        TransformComponent__set_trs__(self._handle, pos, rotation, scale, recursive)
    def set_trs_matrix(self, trs: double4x4, recursive: bool):
        TransformComponent__set_trs_matrix__(self._handle, trs, recursive)
    def trs(self):
        return TransformComponent__trs__(self._handle)
    def trs_float(self):
        return TransformComponent__trs_float__(self._handle)
    def entity(self):
        return Entity(Component__entity__(self._handle))
    def update_data(self):
        Component__update_data__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class LightComponent:
    def __init__(self, handle):
        self._handle = handle


    def add_area_light(self, luminance: float3, visible: bool):
        LightComponent__add_area_light__(self._handle, luminance, visible)
    def add_disk_light(self, luminance: float3, visible: bool):
        LightComponent__add_disk_light__(self._handle, luminance, visible)
    def add_point_light(self, luminance: float3, visible: bool):
        LightComponent__add_point_light__(self._handle, luminance, visible)
    def add_spot_light(self, luminance: float3, angle_radians: float, small_angle_radians: float, angle_atten_pow: float, visible: bool):
        LightComponent__add_spot_light__(self._handle, luminance, angle_radians, small_angle_radians, angle_atten_pow, visible)
    def angle_atten_pow(self):
        return LightComponent__angle_atten_pow__(self._handle)
    def angle_radians(self):
        return LightComponent__angle_radians__(self._handle)
    def luminance(self):
        return LightComponent__luminance__(self._handle)
    def small_angle_radians(self):
        return LightComponent__small_angle_radians__(self._handle)
    def entity(self):
        return Entity(Component__entity__(self._handle))
    def update_data(self):
        Component__update_data__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class Resource:
    def __init__(self, handle):
        self._handle = handle


    def install(self):
        return Resource__install__(self._handle)
    def load(self):
        Resource__load__(self._handle)
    def load_status(self):
        return Resource__load_status__(self._handle)
    def path(self):
        return Resource__path__(self._handle)
    def save_to_path(self):
        return Resource__save_to_path__(self._handle)
    def wait_loading(self):
        Resource__wait_loading__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class TextureResource:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__TextureResource__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def create_empty(self, pixel_storage: LCPixelStorage, size: uint2, mip_level: int, is_virtual_texture: bool):
        TextureResource__create_empty__(self._handle, pixel_storage, size, mip_level, is_virtual_texture)
    def data_buffer(self):
        return TextureResource__data_buffer__(self._handle)
    def has_data_buffer(self):
        return TextureResource__has_data_buffer__(self._handle)
    def heap_index(self):
        return TextureResource__heap_index__(self._handle)
    def is_vt(self):
        return TextureResource__is_vt__(self._handle)
    def load_executed(self):
        return TextureResource__load_executed__(self._handle)
    def mip_level(self):
        return TextureResource__mip_level__(self._handle)
    def pack_to_tile(self):
        return TextureResource__pack_to_tile__(self._handle)
    def pixel_storage(self):
        return TextureResource__pixel_storage__(self._handle)
    def set_skybox(self):
        TextureResource__set_skybox__(self._handle)
    def size(self):
        return TextureResource__size__(self._handle)
    def install(self):
        return Resource__install__(self._handle)
    def load(self):
        Resource__load__(self._handle)
    def load_status(self):
        return Resource__load_status__(self._handle)
    def path(self):
        return Resource__path__(self._handle)
    def save_to_path(self):
        return Resource__save_to_path__(self._handle)
    def wait_loading(self):
        Resource__wait_loading__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class MeshResource:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__MeshResource__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def basic_size_bytes(self):
        return MeshResource__basic_size_bytes__(self._handle)
    def contained_normal(self):
        return MeshResource__contained_normal__(self._handle)
    def contained_tangent(self):
        return MeshResource__contained_tangent__(self._handle)
    def create_as_morphing_instance(self, origin_mesh):
        MeshResource__create_as_morphing_instance__(self._handle, origin_mesh)
    def create_empty(self, submesh_offsets, vertex_count: int, triangle_count: int, uv_count: int, contained_normal: bool, contained_tangent: bool):
        MeshResource__create_empty__(self._handle, submesh_offsets, vertex_count, triangle_count, uv_count, contained_normal, contained_tangent)
    def data_buffer(self):
        return MeshResource__data_buffer__(self._handle)
    def desire_size_bytes(self):
        return MeshResource__desire_size_bytes__(self._handle)
    def extra_size_bytes(self):
        return MeshResource__extra_size_bytes__(self._handle)
    def has_data_buffer(self):
        return MeshResource__has_data_buffer__(self._handle)
    def is_transforming_mesh(self):
        return MeshResource__is_transforming_mesh__(self._handle)
    def submesh_count(self):
        return MeshResource__submesh_count__(self._handle)
    def triangle_count(self):
        return MeshResource__triangle_count__(self._handle)
    def uv_count(self):
        return MeshResource__uv_count__(self._handle)
    def vertex_count(self):
        return MeshResource__vertex_count__(self._handle)
    def install(self):
        return Resource__install__(self._handle)
    def load(self):
        Resource__load__(self._handle)
    def load_status(self):
        return Resource__load_status__(self._handle)
    def path(self):
        return Resource__path__(self._handle)
    def save_to_path(self):
        return Resource__save_to_path__(self._handle)
    def wait_loading(self):
        Resource__wait_loading__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class MaterialResource:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__MaterialResource__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def load_from_json(self, json: str):
        MaterialResource__load_from_json__(self._handle, json)
    def mat_code(self):
        return MaterialResource__mat_code__(self._handle)
    def install(self):
        return Resource__install__(self._handle)
    def load(self):
        Resource__load__(self._handle)
    def load_status(self):
        return Resource__load_status__(self._handle)
    def path(self):
        return Resource__path__(self._handle)
    def save_to_path(self):
        return Resource__save_to_path__(self._handle)
    def wait_loading(self):
        Resource__wait_loading__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class RenderComponent:
    def __init__(self, handle):
        self._handle = handle


    def get_tlas_index(self):
        return RenderComponent__get_tlas_index__(self._handle)
    def remove_object(self):
        RenderComponent__remove_object__(self._handle)
    def update_material(self, mat_vector):
        RenderComponent__update_material__(self._handle, mat_vector)
    def update_mesh(self, mesh: MeshResource):
        RenderComponent__update_mesh__(self._handle, mesh._handle)
    def update_object(self, mat_vector, mesh: MeshResource):
        RenderComponent__update_object__(self._handle, mat_vector, mesh._handle)
    def entity(self):
        return Entity(Component__entity__(self._handle))
    def update_data(self):
        Component__update_data__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class CameraComponent:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__CameraComponent__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def aperture(self):
        return CameraComponent__aperture__(self._handle)
    def aspect_ratio(self):
        return CameraComponent__aspect_ratio__(self._handle)
    def auto_aspect_ratio(self):
        return CameraComponent__auto_aspect_ratio__(self._handle)
    def disable_camera(self):
        CameraComponent__disable_camera__(self._handle)
    def enable_camera(self):
        CameraComponent__enable_camera__(self._handle)
    def enable_physical_camera(self):
        return CameraComponent__enable_physical_camera__(self._handle)
    def far_plane(self):
        return CameraComponent__far_plane__(self._handle)
    def focus_distance(self):
        return CameraComponent__focus_distance__(self._handle)
    def fov(self):
        return CameraComponent__fov__(self._handle)
    def near_plane(self):
        return CameraComponent__near_plane__(self._handle)
    def save_image_to(self, path: str):
        CameraComponent__save_image_to__(self._handle, path)
    def set_aperture(self, value: float):
        CameraComponent__set_aperture__(self._handle, value)
    def set_aspect_ratio(self, value: float):
        CameraComponent__set_aspect_ratio__(self._handle, value)
    def set_auto_aspect_ratio(self, value: float):
        CameraComponent__set_auto_aspect_ratio__(self._handle, value)
    def set_enable_physical_camera(self, value: bool):
        CameraComponent__set_enable_physical_camera__(self._handle, value)
    def set_far_plane(self, value: float):
        CameraComponent__set_far_plane__(self._handle, value)
    def set_focus_distance(self, value: float):
        CameraComponent__set_focus_distance__(self._handle, value)
    def set_fov(self, value: float):
        CameraComponent__set_fov__(self._handle, value)
    def set_near_plane(self, value: float):
        CameraComponent__set_near_plane__(self._handle, value)
    def entity(self):
        return Entity(Component__entity__(self._handle))
    def update_data(self):
        Component__update_data__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class EntitiesCollection:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__EntitiesCollection__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def count(self):
        return EntitiesCollection__count__(self._handle)
    def get_entity(self, index: int):
        return Entity(EntitiesCollection__get_entity__(self._handle, index))



class Scene:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__Scene__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def get_entities_by_name(self, name: str):
        return EntitiesCollection(Scene__get_entities_by_name__(self._handle, name))
    def get_entity(self, guid: GUID):
        return Entity(Scene__get_entity__(self._handle, guid))
    def get_entity_by_name(self, name: str):
        return Entity(Scene__get_entity_by_name__(self._handle, name))
    def get_or_add_entity(self, guid: GUID):
        return Entity(Scene__get_or_add_entity__(self._handle, guid))
    def remove_entity(self, guid: GUID):
        Scene__remove_entity__(self._handle, guid)
    def update_data(self):
        Scene__update_data__(self._handle)
    def install(self):
        return Resource__install__(self._handle)
    def load(self):
        Resource__load__(self._handle)
    def load_status(self):
        return Resource__load_status__(self._handle)
    def path(self):
        return Resource__path__(self._handle)
    def save_to_path(self):
        return Resource__save_to_path__(self._handle)
    def wait_loading(self):
        Resource__wait_loading__(self._handle)
    def base_type(self):
        return Object__base_type__(self._handle)
    def guid(self):
        return Object__guid__(self._handle)
    def type_id(self):
        return Object__type_id__(self._handle)
    def type_name(self):
        return Object__type_name__(self._handle)



class FileMeta:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__FileMeta__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def guid(self):
        return FileMeta__guid__(self._handle)
    def meta_json(self):
        return FileMeta__meta_json__(self._handle)



class Project:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__Project__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def get_file_meta(self, type_id: GUID, dest_path: str):
        return FileMeta(Project__get_file_meta__(self._handle, type_id, dest_path))
    def import_material(self, path: str, extra_meta: str):
        Project__import_material__(self._handle, path, extra_meta)
    def import_mesh(self, path: str, extra_meta: str):
        Project__import_mesh__(self._handle, path, extra_meta)
    def import_scene(self, path: str, extra_meta: str):
        return Scene(Project__import_scene__(self._handle, path, extra_meta))
    def import_texture(self, path: str, extra_meta: str):
        Project__import_texture__(self._handle, path, extra_meta)
    def init(self, assets_root_dir: str):
        Project__init__(self._handle, assets_root_dir)
    def load_resource(self, guid: GUID, async_load: bool):
        return TextureResource(Project__load_resource__(self._handle, guid, async_load))
    def scan_project(self):
        Project__scan_project__(self._handle)


# ================== GENERATED CODE END ==================
