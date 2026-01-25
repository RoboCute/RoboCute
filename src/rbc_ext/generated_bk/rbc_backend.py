
# This File is Generated From Python Def
# Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
# ================== GENERATED CODE BEGIN ==================
from rbc_ext._C.test_py_codegen import *




class RBCContext:
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__RBCContext__()
        else:
            self._handle = args[0]

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)

    def create_window(self, name: str, size: uint2, resizable: bool):
        RBCContext__create_window__(self._handle, name, size, resizable)
    def denoise(self):
        RBCContext__denoise__(self._handle)
    def disable_view(self):
        RBCContext__disable_view__(self._handle)
    def init_device(self, rhi_backend: str, program_path: str, shader_path: str):
        RBCContext__init_device__(self._handle, rhi_backend, program_path, shader_path)
    def init_render(self):
        RBCContext__init_render__(self._handle)
    def init_world(self, meta_path: str, binary_path: str):
        RBCContext__init_world__(self._handle, meta_path, binary_path)
    def reset_view(self, resolution: uint2):
        RBCContext__reset_view__(self._handle, resolution)
    def save_display_image_to(self, path: str):
        RBCContext__save_display_image_to__(self._handle, path)
    def set_view_camera(self, pos: float3, roll: float, pitch: float, yaw: float):
        RBCContext__set_view_camera__(self._handle, pos, roll, pitch, yaw)
    def should_close(self):
        return RBCContext__should_close__(self._handle)
    def tick(self, delta_time: float, resolution: uint2, frame_index: int, tick_stage: TickStage, prepare_denoise: bool):
        RBCContext__tick__(self._handle, delta_time, resolution, frame_index, tick_stage, prepare_denoise)


# ================== GENERATED CODE END ==================
