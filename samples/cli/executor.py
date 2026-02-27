import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re


def _reflect_function(func, func_name):
    import inspect
    sig = inspect.signature(func)
    d = []
    ret_anno = sig.return_annotation
    for name, param in sig.parameters.items():
        anno = param.annotation
        if anno == inspect._empty:
            raise Exception(
                f"Error: function '{func_name}' argument '{param.name}' has no annotation.")
        d.append(param.annotation)
    return d


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
        return func_name, func_value

    def add_function(self, name: str, func):
        if not callable(func):
            raise Exception(f"Error: function '{name}' not callable.")
        # {'name': <class 'str'>, 'age': <class 'int'>, 'return': <class 'bool'>}
        reflected = _reflect_function(func, name)
        self._func_table[name] = [reflected, func]

    def execute_cli(self, input_func, end_func):
        while True:
            command: str = input_func('Input next call: ')
            command = command.strip()
            if end_func(command):
                break
            try:
                _func_nameac70bb6f, _func_valuec9003d9f, = self._check_cmd(
                    command, self._context)
                self._context[_func_nameac70bb6f] = _func_valuec9003d9f[1]
                exec(command, self._context)
                del self._context[_func_nameac70bb6f]
                yield ''
            except Exception as e:
                yield str(e)


if __name__ == '__main__':
    def example(name: str, value: float, age: int = 18):
        value = (str(value) + ' ' + name + ' ' + str(age))
        return value

    def my_print(s: str):
        print(s)

    tb = CLITable()

    def add_func(tb: CLITable, name: str):
        tb.add_function(name, eval(name))

    add_func(tb, 'example')
    add_func(tb, 'my_print')
    f = tb.execute_cli(
        input,
        lambda c: c == 'exit'
    )
    while True:
        try:
            value = next(f)
            print('Result: ' + str(value))
        except StopIteration as e:
            break
