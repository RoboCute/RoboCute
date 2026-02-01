from pathlib import Path
import os
import re
import hashlib


# Write Result String To
def _write_string_to(s: str, path: Path):
    data = s.encode("utf-8")
    new_md5 = hashlib.md5(data).hexdigest()
    print(f"Writing to {path}")
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


def codegen_to(out_path: Path):
    def _(callback, *args):
        s = callback(*args)
        _write_string_to(s, out_path)

    return _


def re_translate(text: str, replacements: dict):
    pattern = re.compile(
        "|".join(re.escape(k) for k in sorted(replacements, key=len, reverse=True))
    )
    updated_text = pattern.sub(lambda match: replacements[match.group(0)], text)
    return updated_text


def _print_str(t, py_interface: bool = False, is_view: bool = False) -> str:
    if is_view:
        return "luisa::string_view"
    elif py_interface:
        return "luisa::string"
    else:
        return "luisa::string"


def _print_guid(t, py_interface: bool = False, is_view: bool = False) -> str:
    if py_interface:
        return "GuidData"
    elif is_view:
        return "vstd::Guid const&"
    else:
        return "vstd::Guid"


def _print_data_buffer(t, py_interface: bool = False, is_view: bool = False) -> str:
    if py_interface:
        if is_view:
            return "py::buffer const&"
        else:
            return "py::memoryview"
    else:
        return "luisa::span<std::byte>"


def _print_callback(t, py_interface: bool = False, is_view: bool = False) -> str:
    if py_interface:
        if is_view:
            return "py::function const&"
        else:
            raise ImportError("callback from c++ not supported.")
    else:
        return "luisa::function<void()> const&"
