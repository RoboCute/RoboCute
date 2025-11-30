import rbc_meta.utils.type_register as tr
import rbc_meta.utils.codegen_util as ut
import rbc_meta.utils.codegen as codegen
from pathlib import Path


def codegen_module(cpp_root_path: Path, py_root_path: Path):
    pyd_name = "test_py_codegen"
    file_name = "example_module"
    ut.codegen_pyd_module(
        pyd_name, file_name, cpp_root_path, py_root_path
    )
