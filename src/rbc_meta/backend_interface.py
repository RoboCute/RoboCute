import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen_util as ut
import rbc_meta.resource_enums as res_enums
from pathlib import Path


def codegen_header(
    cpp_root_path: Path,
    header_root_path: Path,
    py_root_path: Path,
):
    pyd_name = 'test_py_codegen'
    file_name = 'rbc_backend'
    Context = tr.struct('RBCContext', "TEST_GRAPHICS_API")
    # these enums defined in rbc_runtime
    res_enums.mark_enums_external()
    # frame
    Context.method(
        'init_device',
        rhi_backend=tr.string,
        program_path=tr.string,
        shader_path=tr.string)
    # render
    Context.method('init_render')
    Context.method('load_skybox', path=tr.string, size=tr.uint2)
    Context.method('create_window', name=tr.string,
                   size=tr.uint2, resizable=tr.bool)
    # mesh
    Context.method('create_mesh', data=tr.DataBuffer, vertex_count=tr.uint,
                   contained_normal=tr.bool, contained_tangent=tr.bool, uv_count=tr.uint, triangle_count=tr.uint
                   # TODO: submesh
                   ).ret_type(tr.VoidPtr)
    Context.method('remove_mesh', handle=tr.VoidPtr)
    # light
    Context.method(
        'add_area_light', matrix=tr.float4x4,
        luminance=tr.float3, visible=tr.bool).ret_type(tr.VoidPtr)
    Context.method(
        'add_disk_light', center=tr.float3, radius=tr.float,
        luminance=tr.float3, forward_dir=tr.float3, visible=tr.bool).ret_type(tr.VoidPtr)
    Context.method(
        'add_point_light', center=tr.float3,
        radius=tr.float, luminance=tr.float3, visible=tr.bool).ret_type(tr.VoidPtr)
    Context.method(
        'add_spot_light', center=tr.float3, radius=tr.float, luminance=tr.float3, forward_dir=tr.float3,
        angle_radians=tr.float, small_angle_radians=tr.float, angle_atten_pow=tr.float, visible=tr.bool).ret_type(tr.VoidPtr)
    Context.method(
        'update_area_light', light=tr.VoidPtr, matrix=tr.float4x4,
        luminance=tr.float3, visible=tr.bool)
    Context.method(
        'update_disk_light', light=tr.VoidPtr, center=tr.float3, radius=tr.float,
        luminance=tr.float3, forward_dir=tr.float3, visible=tr.bool)
    Context.method(
        'update_point_light', light=tr.VoidPtr, center=tr.float3,
        radius=tr.float, luminance=tr.float3, visible=tr.bool)
    Context.method(
        'update_spot_light', light=tr.VoidPtr, center=tr.float3, radius=tr.float, luminance=tr.float3, forward_dir=tr.float3,
        angle_radians=tr.float, small_angle_radians=tr.float, angle_atten_pow=tr.float, visible=tr.bool)
    Context.method(
        'remove_light', light=tr.VoidPtr
    )
    # texture
    Context.method(
        'create_texture',
        data=tr.DataBuffer,
        storage = res_enums.PixelStorage,
        size=tr.uint2,
        address=res_enums.SamplerAddress,
        filter=res_enums.SamplerFilter,
        mip_level=tr.uint,
        is_virtual_texture=tr.bool
    ).ret_type(tr.VoidPtr)
    Context.method(
        'texture_heap_idx',
        ptr = tr.VoidPtr
    ).ret_type(tr.uint)
    Context.method(
        'destroy_texture',
        ptr = tr.VoidPtr
    )
    # object
    Context.method(
        'create_object',
        matrix=tr.float4x4,
        mesh=tr.VoidPtr
        # TODO: material
    ).ret_type(tr.VoidPtr)

    Context.method(
        'update_object',
        matrix=tr.float4x4,
        # TODO: material
    )
    Context.method(
        'update_object',
        matrix=tr.float4x4,
        mesh=tr.VoidPtr
        # TODO: material
    )
    Context.method(
        'remove_object',
        object_ptr=tr.VoidPtr
    )

    # view
    Context.method(
        'reset_view',
        resolution=tr.uint2
    )
    Context.method(
        'set_view_camera',
        pos=tr.float3,
        roll=tr.float,
        pitch=tr.float,
        yaw=tr.float
    )
    Context.method(
        'disable_view'
    )
    # tick
    Context.method(
        'tick'
    )
    Context.method(
        'should_close'
    ).ret_type(tr.bool)

    # codegen
    ut.codegen_to(header_root_path / f"{file_name}.h")(ut.codegen.cpp_interface_gen, '#include <rbc_runtime/generated/resource_meta.hpp>')
    ut.codegen_to(cpp_root_path / f"{file_name}.cpp")(
        ut.codegen.pybind_codegen,
        file_name,
        f'#include "{file_name}.h"',
    )
    ut.codegen_to(py_root_path / f"{file_name}.py")(ut.codegen.py_interface_gen, pyd_name)
