import type_register as tr
from pathlib import Path
import os
import codegen


def _write_string_to(s: str, path: str):
    bytes = s.encode("ascii")
    if os.path.exists(path):
        size = os.path.getsize(path)
        if size == len(bytes):
            f = open(path, "rb")
            src_bytes = f.read()
            f.close()
            if src_bytes == bytes:
                return False
    f = open(path, "wb")
    f.write(bytes)
    f.close()
    return True


def codegen_to(out_path: str, callback, *args):
    s = callback(*args)
    if not isinstance(s, str):
        tr.log_err("codegen require callback return string.")
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    _write_string_to(s, out_path)


def codegen_pyd_module(
    pyd_name: str, file_name: str, backend_module_name: str, cpp_root_path: Path,
    py_root_path: Path
):
    codegen_to(cpp_root_path / f"{file_name}.h", codegen.cpp_interface_gen)
    codegen_to(
        cpp_root_path / f"{file_name}.cpp",
        codegen.nanobind_codegen,
        file_name,
        f"{backend_module_name}",
        f'#include "{file_name}.h"',
    )
    codegen_to(py_root_path / f"{file_name}.py", codegen.py_interface_gen, pyd_name)
