_line_indent = 0
_result = ""


def add_line(line: str):
    global _result, _line_indent
    _result += "\n"
    for _ in range(_line_indent):
        _result += "\t"
    _result += line


def set_result(s: str):
    global _result
    _result = s


def add_result(s: str):
    global _result
    _result += s


def get_result():
    return _result


def add_indent():
    global _line_indent
    _line_indent += 1


def remove_indent():
    global _line_indent
    _line_indent -= 1
