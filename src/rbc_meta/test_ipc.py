import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen as codegen
import rbc_meta.utils.codegen_util as ut
from pathlib import Path


def codegen_header(header_path: Path):
    Chat = tr.struct("Chat")
    Chat.rpc("chat", True, value=tr.string).ret_type(tr.string)
    Chat.rpc("exit", True)
