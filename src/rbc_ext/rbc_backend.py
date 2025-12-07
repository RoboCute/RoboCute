from rbc_ext._C.test_py_codegen import *

class RBCContext:
    def __init__(self):
        self._handle = create__RBCContext__()

    def __del__(self):
        dispose__RBCContext__(self._handle)

    def add_area_light(self, matrix: float4x4, luminance: float3, visible: bool):
        return RBCContext__add_area_light__(self._handle, matrix, luminance, visible)
    def add_disk_light(self, center: float3, radius: float, luminance: float3, forward_dir: float3, visible: bool):
        return RBCContext__add_disk_light__(self._handle, center, radius, luminance, forward_dir, visible)
    def add_point_light(self, center: float3, radius: float, luminance: float3, visible: bool):
        return RBCContext__add_point_light__(self._handle, center, radius, luminance, visible)
    def add_spot_light(self, center: float3, radius: float, luminance: float3, forward_dir: float3, angle_radians: float, small_angle_radians: float, angle_atten_pow: float, visible: bool):
        return RBCContext__add_spot_light__(self._handle, center, radius, luminance, forward_dir, angle_radians, small_angle_radians, angle_atten_pow, visible)
    def create_mesh(self, vertex_count: int, contained_normal: bool, contained_tangent: bool, uv_count: int, triangle_count: int, offsets_uint32):
        return RBCContext__create_mesh__(self._handle, vertex_count, contained_normal, contained_tangent, uv_count, triangle_count, offsets_uint32)
    def create_object(self, matrix: float4x4, mesh, materials):
        return RBCContext__create_object__(self._handle, matrix, mesh, materials)
    def create_pbr_material(self):
        return RBCContext__create_pbr_material__(self._handle)
    def create_texture(self, storage: LCPixelStorage, size: uint2, mip_level: int):
        return RBCContext__create_texture__(self._handle, storage, size, mip_level)
    def create_window(self, name: str, size: uint2, resizable: bool):
        RBCContext__create_window__(self._handle, name, size, resizable)
    def disable_view(self):
        RBCContext__disable_view__(self._handle)
    def get_material_json(self, mat):
        return RBCContext__get_material_json__(self._handle, mat)
    def get_mesh_data(self, handle):
        return RBCContext__get_mesh_data__(self._handle, handle)
    def get_texture_data(self, handle):
        return RBCContext__get_texture_data__(self._handle, handle)
    def init_device(self, rhi_backend: str, program_path: str, shader_path: str):
        RBCContext__init_device__(self._handle, rhi_backend, program_path, shader_path)
    def init_render(self):
        RBCContext__init_render__(self._handle)
    def load_mesh(self, file_path: str, file_offset: int, vertex_count: int, contained_normal: bool, contained_tangent: bool, uv_count: int, triangle_count: int, offsets_uint32):
        return RBCContext__load_mesh__(self._handle, file_path, file_offset, vertex_count, contained_normal, contained_tangent, uv_count, triangle_count, offsets_uint32)
    def load_skybox(self, path: str, size: uint2):
        RBCContext__load_skybox__(self._handle, path, size)
    def load_texture(self, path: str, file_offset: int, storage: LCPixelStorage, size: uint2, mip_level: int, is_virtual_texture: bool):
        return RBCContext__load_texture__(self._handle, path, file_offset, storage, size, mip_level, is_virtual_texture)
    def reset_frame_index(self):
        RBCContext__reset_frame_index__(self._handle)
    def reset_view(self, resolution: uint2):
        RBCContext__reset_view__(self._handle, resolution)
    def set_view_camera(self, pos: float3, roll: float, pitch: float, yaw: float):
        RBCContext__set_view_camera__(self._handle, pos, roll, pitch, yaw)
    def should_close(self):
        return RBCContext__should_close__(self._handle)
    def texture_heap_idx(self, ptr):
        return RBCContext__texture_heap_idx__(self._handle, ptr)
    def tick(self):
        RBCContext__tick__(self._handle)
    def update_area_light(self, light, matrix: float4x4, luminance: float3, visible: bool):
        RBCContext__update_area_light__(self._handle, light, matrix, luminance, visible)
    def update_disk_light(self, light, center: float3, radius: float, luminance: float3, forward_dir: float3, visible: bool):
        RBCContext__update_disk_light__(self._handle, light, center, radius, luminance, forward_dir, visible)
    def update_material(self, mat_ptr, json: str):
        RBCContext__update_material__(self._handle, mat_ptr, json)
    def update_mesh(self, handle, only_vertex: bool):
        RBCContext__update_mesh__(self._handle, handle, only_vertex)
    def update_object(self, object_ptr, matrix: float4x4, mesh, materials):
        RBCContext__update_object__(self._handle, object_ptr, matrix, mesh, materials)
    def update_object_pos(self, object_ptr, matrix: float4x4):
        RBCContext__update_object_pos__(self._handle, object_ptr, matrix)
    def update_point_light(self, light, center: float3, radius: float, luminance: float3, visible: bool):
        RBCContext__update_point_light__(self._handle, light, center, radius, luminance, visible)
    def update_spot_light(self, light, center: float3, radius: float, luminance: float3, forward_dir: float3, angle_radians: float, small_angle_radians: float, angle_atten_pow: float, visible: bool):
        RBCContext__update_spot_light__(self._handle, light, center, radius, luminance, forward_dir, angle_radians, small_angle_radians, angle_atten_pow, visible)
    def update_texture(self, handle):
        RBCContext__update_texture__(self._handle, handle)

