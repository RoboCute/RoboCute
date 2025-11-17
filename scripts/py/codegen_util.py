import type_register as tr
import sys
from pathlib import Path
import os

def _write_string_to(s: str, path: str):
    bytes = s.encode('ascii')
    if os.path.exists(path):
        size = os.path.getsize(path)
        if size == len(bytes):
            f = open(path, 'rb')
            src_bytes = f.read()
            f.close()
            if src_bytes == bytes:
                return False
    f = open(path, 'wb')
    f.write(bytes)
    f.close()
    return True


def codegen(
    callback,
    *args
):
    s = callback(*args)
    if type(s) != str:
        tr.log_err(f'codegen require callback return string.')
    if len(sys.argv) < 2:
        tr.log_err(f'Should pass out_path from arguments.')
    out_path = Path(sys.argv[1])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    _write_string_to(s, out_path)


def codegen_to(
    out_path: str,
    callback,
    *args
):
    s = callback(*args)
    if type(s) != str:
        tr.log_err(f'codegen require callback return string.')
    Path(out_path).parent.mkdir(parents = True, exist_ok=True)
    _write_string_to(s, out_path)
