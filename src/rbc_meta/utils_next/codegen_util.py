from pathlib import Path
import os
import re

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
        _write_string_to(s, str(out_path))

    return _


def re_translate(text: str, replacements: dict):
    pattern = re.compile(
        "|".join(re.escape(k) for k in sorted(replacements, key=len, reverse=True))
    )
    updated_text = pattern.sub(lambda match: replacements[match.group(0)], text)
    return updated_text
