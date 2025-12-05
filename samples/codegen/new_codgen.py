from rbc_meta.utils_next.builtin import ubyte, ulong
import inspect
from rbc_meta.utils_next import reflect, CodeGenerator


@reflect
class A:
    a: ubyte
    b: ulong


generator = CodeGenerator()
python_code = generator.generate_python("A")
print(python_code)
