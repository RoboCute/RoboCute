import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen as codegen
import rbc_meta.utils.codegen_util as ut
from pathlib import Path


def codegen_header(header_path: Path):
    Chat = tr.struct('Chat')
    Chat.rpc("chat", True, value=tr.string).ret_type(tr.string)
    Chat.rpc("exit", True)
    
    ut.codegen_to(header_path / "server.hpp")(codegen.cpp_interface_gen)

    include = '#include "server.hpp"'
    ut.codegen_to(header_path / "server.cpp")(codegen.cpp_impl_gen, include)
    
    client_path = header_path / 'client.hpp'
    ut.codegen_to(client_path)(codegen.cpp_client_interface_gen)
    include = '#include "client.hpp"'
    client_path = header_path / 'client.cpp'
    ut.codegen_to(client_path)(codegen.cpp_client_impl_gen, include)