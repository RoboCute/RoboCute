# The Singleton Code Generator
class CodeGenerator:

    def __init__(cls):
        cls._line_indent = 0
        cls._result = ""

    def add_line(self, line: str):
        self._result += "\n"
        for _ in range(self._line_indent):
            self._result += "\t"
        self._result += line

    def set_result(self, s: str):
        self._result = s
        self._line_indent = 0

    def add_result(self, s: str):
        self._result += s

    def get_result(self):
        return self._result

    def add_indent(self):
        self._line_indent += 1

    def remove_indent(self):
        self._line_indent -= 1


def get_codegen_basis():
    return CodeGenerator()
