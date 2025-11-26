import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen as codegen
import rbc_meta.utils.codegen_util as ut
from pathlib import Path


def codegen_header(backend_path: Path, frontend_path: Path):
    SharedWindow = tr.struct('rbc::SharedWindow')
    SharedWindow.members(
        device=tr.external_type('luisa::compute::Device*'),
        stream=tr.external_type('luisa::compute::Stream*'),
        dirty=tr.bool,
        swapchains=tr.external_type('luisa::unordered_map<uint64_t, luisa::compute::Swapchain>')
    )
    SharedWindow.rpc(
        'open_window',
        window_handle=tr.ulong
    )
    SharedWindow.rpc(
        'close_window',
        window_handle=tr.ulong
    )
    SharedWindow.add_default_ctor()
    SharedWindow.add_default_dtor()

    # print backend
    include = '''#include <luisa/runtime/swapchain.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
'''
    ut.codegen_to(
        backend_path / "generated.hpp")(codegen.cpp_interface_gen, include)

    include = '#include "generated.hpp"'
    ut.codegen_to(backend_path /
                  "generated.cpp")(codegen.cpp_impl_gen, include)

    # print frontend
    client_path = frontend_path / 'client.hpp'
    ut.codegen_to(client_path)(codegen.cpp_client_interface_gen)
    include = '#include "client.hpp"'
    client_path = frontend_path / 'client.cpp'
    ut.codegen_to(client_path)(codegen.cpp_client_impl_gen, include)
