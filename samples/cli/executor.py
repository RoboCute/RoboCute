import inspect


def _reflect_function(func, func_name):
    sig = inspect.signature(func)
    types = []
    names = []
    for name, param in sig.parameters.items():
        anno = param.annotation
        names.append(name)
        if anno == inspect._empty:
            raise Exception(
                f"Error: function '{func_name}' argument '{param.name}' has no annotation.")
        types.append(param.annotation)
    return types, names


def _parse_function(code: str):
    """
    Parse a Python-style function call string.

    Args:
        code: A string containing a function call, optionally with assignment.
            Examples: 'func_call(arg0, arg1)' or 'a = func_call(arg0, arg1)'

    Returns:
        list: A list containing [return_var, func_name, [arg0, arg1, ...]]
            return_var is None if no assignment is present.
    """
    import ast

    code = code.strip()
    if not code:
        return [None, None, []]

    tree = ast.parse(code)
    if not tree.body:
        raise Exception(
            'Error: failed to parse the command. The input is empty or '
            'contains invalid syntax.'
        )
    if isinstance(tree.body, ast.Assign):
        my_body = tree.body
    else:
        my_body = tree.body[0]
    call_node = my_body.value
    return_var = None

    # Handle assignment: a = func_call(...)
    if isinstance(my_body, ast.Assign):
        if len(my_body.targets) == 1 and isinstance(
            my_body.targets[0], ast.Name
        ):
            return_var = my_body.targets[0].id
        call_node = my_body.value

    if not isinstance(call_node, ast.Call):
        return [None, None, []]

    # Extract function name
    if isinstance(call_node.func, ast.Name):
        func_name = call_node.func.id
    elif isinstance(call_node.func, ast.Attribute):
        func_name = call_node.func.attr
    else:
        func_name = None

    # Extract arguments
    args = []
    for arg in call_node.args:
        if isinstance(arg, ast.Name):
            args.append(arg.id)
        elif isinstance(arg, ast.Constant):
            if isinstance(arg.value, str):
                args.append(repr(arg.value))
            else:
                args.append(str(arg.value))
        else:
            args.append(ast.unparse(arg) if hasattr(
                ast, 'unparse') else str(arg))

    return [return_var, func_name, args]


_num = {float, int}


def _is_num(type_val: type):
    global _num
    return type_val in _num


def _check_args(
    func_name: str,
    type_lists: list,
    args: list
):
    if len(args) != len(type_lists):
        raise Exception(
            f"Error: function '{func_name}' expects {len(type_lists)} "
            f"argument(s), but {len(args)} provided."
        )
    for i in range(len(args)):
        t = args[i]
        dst_t = type_lists[i]
        if _is_num(t) and _is_num(dst_t):
            continue
        if t != dst_t:
            raise Exception(
                f"Error: function '{func_name}' argument {i + 1} type "
                f"mismatch. Expected '{dst_t.__name__}', got "
                f"'{t.__name__}'."
            )


class CLITable:
    def __init__(self):
        # name : [function: func, {arg_count}]
        self._func_table = {}
        self._context = {}
        exec('''
from robocute.rbc_ext.luisa import (
    # Vector types
    float2,
    float3,
    float4,
    int2,
    int3,
    int4,
    uint2,
    uint3,
    uint4,
    double2,
    double3,
    double4,
    bool2,
    bool3,
    bool4,
    half2,
    half3,
    half4,
    short2,
    short3,
    short4,
    ushort2,
    ushort3,
    ushort4,
    # Matrix types
    float2x2,
    float3x3,
    float4x4,
    # make functions for vectors and matrices
    make_float2,
    make_float3,
    make_float4,
    make_int2,
    make_int3,
    make_int4,
    make_bool2,
    make_bool3,
    make_bool4,
    make_float2x2,
    make_float3x3,
    make_float4x4,
)

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
from robocute.rbc_ext._C import lcapi_c as lcapi
''', self._context)
        pass

    def _check_cmd(
        self,
        command: str,
        ctx: dict
    ):
        cmd_infos = _parse_function(command)
        func_name = cmd_infos[1]
        if func_name is None:
            raise Exception(
                'Error: invalid function call. Please check your '
                'command syntax.'
            )
        func_value = self._func_table.get(func_name)
        if func_value is None:
            raise Exception(
                f"Error: function '{func_name}' not found. "
            )
        types = func_value[0]
        # types: {'name': <class 'str'>, 'value': <class 'float'>, 'age': <class 'int'>, 'return': None}
        values = cmd_infos[2]
        # arg_values: ['1.0f', '2.0', 'adfadsf', '33', "'''aabb'''", 'lc.float4(1, 2, 3, 4)']
        value_types = []
        for i in values:
            value_types.append(type(eval(i, ctx)))
        _check_args(func_name, types, value_types)
        return func_name, func_value, cmd_infos[0]

    def add_function(self, name: str, func, doc: str = None):
        if not callable(func):
            raise Exception(f"Error: function '{name}' not callable.")
        # {'name': <class 'str'>, 'age': <class 'int'>, 'return': <class 'bool'>}
        reflected, arg_names = _reflect_function(func, name)
        self._func_table[name] = [reflected, func, doc, arg_names]

    def execute_cli(self, input_func, end_func):
        while True:
            command: str = None
            command_coro = input_func('Input next call:\n')
            if inspect.isgenerator(command_coro):
                while True:
                    try:
                        v = next(command_coro)
                        if type(v) == str:
                            command = v
                            break
                        yield None
                    except StopIteration:
                        break
            assert type(command) == str
            command = command.strip()
            if end_func(command):
                break
            try:
                func_name, func_value, ret_name = self._check_cmd(
                    command, self._context)
                self._context[func_name] = func_value[1]
                exec(command, self._context)
                ret_val = self._context[ret_name]
                if ret_val and inspect.isgenerator(ret_val):
                    while True:
                        try:
                            next(ret_val)
                        except StopIteration:
                            break
                        yield None
                del self._context[func_name]
                yield None
            except Exception as e:
                yield str(e)

    def dump_func_table(self):
        s = ''
        for k, v in self._func_table.items():
            s += f"'{k}': ["
            is_first = True
            for t in v[0]:
                if not is_first:
                    s += ', '
                is_first = False
                s += t.__name__
            s += ']'
            if len(v) >= 2 and v[2] is not None:
                s += '    # Description: ' + v[2]
            s += '\n'
        return s


def async_input(prompt: str = ''):
    """
    Asynchronous input using yield as coroutine.

    This function yields None while waiting for input, allowing other
    tasks to run concurrently. When input is available, it yields the
    input string.

    Args:
        prompt: The prompt string to display to the user.

    Yields:
        None while waiting for input.
        str when input is received.
    """
    import sys
    import threading
    import queue

    user_input = None

    def read_input():
        nonlocal user_input
        user_input = input(prompt)
    # Start input thread
    thread = threading.Thread(target=read_input, daemon=True)
    thread.start()

    # Yield None while waiting for input
    while thread.is_alive():
        yield None
    yield user_input


if __name__ == '__main__':
    import time

    def example(name: str, value: float, age: int = 18):
        value = (str(value) + ' ' + name + ' ' + str(age))
        return value

    def my_print(s: str):
        print(s)

    tb = CLITable()

    def add_func(tb: CLITable, name: str, doc: str = None):
        tb.add_function(name, eval(name), doc)

    add_func(tb, 'example', 'This is an example function')
    add_func(tb, 'my_print', 'This is a printer')
    f = tb.execute_cli(
        async_input,
        lambda c: c == 'exit'
    )
    print(tb.dump_func_table())
    while True:
        try:
            value = next(f)
            if value:
                print(value)
            time.sleep(0.01)
        except StopIteration as e:
            break
