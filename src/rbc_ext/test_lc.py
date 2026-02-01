from rbc_ext.luisa import *
from rbc_ext.generated.rbc_backend import *
from rbc_ext.luisa.globalvars import *
from rbc_ext.luisa.types import *
import os
from pathlib import Path
import sys
import numpy as np
import logging
import torch

# Auto-setup RBC_RUNTIME_DIR if not set
if "RBC_RUNTIME_DIR" not in os.environ:
    project_root = Path(__file__).parent.parent.parent
    # Try to find the build directory
    found = False
    runtime_dir = project_root / "build" / "windows" / "x64" / "debug"
    if runtime_dir.exists():
        os.environ["RBC_RUNTIME_DIR"] = str(runtime_dir)
        # Also add to PATH for DLL loading
        os.environ["PATH"] = f"{runtime_dir};{os.environ.get('PATH', '')}"
        print(f"Auto-detected RBC_RUNTIME_DIR: {runtime_dir}")
        found = True
    if not found:
        raise RuntimeError(
            f"Could not auto-detect RBC_RUNTIME_DIR. "
            f"Searched in: {project_root / 'build' / 'windows' / 'x64' / 'debug'}"
        )


backend_name = "dx"
runtime_dir = Path(os.getenv("RBC_RUNTIME_DIR"))
program_path = str(runtime_dir.parent / "debug")
shader_path = str(runtime_dir.parent / f"shader_build_{backend_name}")
ctx = RBCContext()
ctx.init_device(backend_name, program_path, shader_path)
init()
if not torch.cuda.is_available():
    logging.error("CUDA environment unavailable.")
    exit(1)
device = torch.device("cuda")
BUFFER_SIZE = 16
arr = np.zeros(BUFFER_SIZE, dtype=np.float32)

interop_buffer = Buffer.import_native(float, get_global_device().create_interop_buffer(to_lctype(float), BUFFER_SIZE))
# Same as:
# interop_buffer = Buffer(BUFFER_SIZE, float, enable_interop=True)

# texture 
# tex_desc = get_global_device().create_texture(lcapi.PixelFormat.RGBA16UNorm, 3, 256, 256,256, 1)
# test_texture = Image3D.import_native(float, tex_desc)

# interop torch and lc
torch_tensor = torch.rand(BUFFER_SIZE, dtype=torch.float32, device=device)
tensor_pointer = torch_tensor.data_ptr()
default_stream = torch.cuda.default_stream()
interop_buffer.interop_copy_from(tensor_pointer, default_stream.cuda_stream)
interop_buffer.copy_to(arr)
print('Printing torch tensor from lc')
for i in arr:
    print(i)
@func
def kernel():
    id = dispatch_id().x
    interop_buffer.write(id, float(id) + 0.114)
kernel(dispatch_size=(BUFFER_SIZE,1,1))
interop_buffer.interop_copy_to(tensor_pointer, default_stream.cuda_stream)
result_arr = torch_tensor.cpu().numpy()

print('Printing lc tensor from torch')
for i in result_arr:
    print(i)
del interop_buffer
del ctx