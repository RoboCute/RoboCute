import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen as codegen

from pathlib import Path
import os

import hashlib


# Write Result String To
def _write_string_to(s: str, path: str):
    data = s.encode("utf-8")
    new_md5 = hashlib.md5(data).hexdigest()

    if os.path.exists(path):
        with open(path, "rb") as f:
            old_data = f.read()
            old_md5 = hashlib.md5(old_data).hexdigest()

        if new_md5 == old_md5:
            return False

    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return True


def codegen_to(out_path: str):
    def _(callback, *args):
        s = callback(*args)

        if not isinstance(s, str):
            tr.log_err("codegen require callback return string.")
        _write_string_to(s, str(out_path))

    return _


def codegen_pyd_module(
    pyd_name: str,
    file_name: str,
    backend_module_name: str,
    cpp_root_path: Path,
    py_root_path: Path,
):
    codegen_to(cpp_root_path / f"{file_name}.h")(codegen.cpp_interface_gen)
    codegen_to(cpp_root_path / f"{file_name}.cpp")(
        codegen.nanobind_codegen,
        file_name,
        f"{backend_module_name}",
        f'#include "{file_name}.h"',
    )
    codegen_to(py_root_path / f"{file_name}.py")(codegen.py_interface_gen, pyd_name)
