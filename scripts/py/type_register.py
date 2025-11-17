import os
from pathlib import Path
# basic types


class ClassPtr:
    pass


class void:
    pass


class byte:
    pass


class short:
    pass


class int:
    pass


class long:
    pass


class ubyte:
    pass


class ushort:
    pass


class uint:
    pass


class ulong:
    pass


class float:
    pass


class double:
    pass


class string:
    pass


class int2:
    pass


class int3:
    pass


class int4:
    pass


class uint2:
    pass


class uint3:
    pass


class uint4:
    pass


class long2:
    pass


class long3:
    pass


class long4:
    pass


class ulong2:
    pass


class ulong3:
    pass


class ulong4:
    pass


class float2:
    pass


class float3:
    pass


class float4:
    pass


class double2:
    pass


class double3:
    pass


class double4:
    pass


class float2x2:
    pass


class float3x3:
    pass


class float4x4:
    pass


class double2x2:
    pass


class double3x3:
    pass


class double4x4:
    pass


class vector:
    def __init__(self, element):
        # TODO: check type
        self._elements = [element]


_basic_types = {byte, short, int, long, ubyte, ushort, uint, ulong, float, double, string, int2, int3, int4, uint2, uint3, uint4, long2, long3, ClassPtr,
                long4, ulong2, ulong3, ulong4, float2, float3, float4, double2, double3, double4, float2x2, float3x3, float4x4, double2x2, double3x3, double4x4}
_template_types = {vector}

_registed_struct_types = {}
_registed_global_funcs = {}


def log_err(s: str):
    print(f"\033[31m{s}\033[0m")
    exit(1)


def _gen_args_key(**args):
    key = ""
    for i in args:
        key += str(args[i]) + '|'
    return key


class _function_t:
    def __init__(self, **args):
        _check_args(**args)
        self._args = args
        self._ret_type = None

    def ret_type(self, ret_type):
        _check_ret_type(ret_type)
        self._ret_type = ret_type
        return self


class struct_t:
    def __init__(self, _func_name: str):
        if _registed_struct_types.get(_func_name):
            log_err(f"Struct {_func_name} already exists.")
        _registed_struct_types[_func_name] = self
        self._name = _func_name
        self._doc = _func_name
        self._member = dict()

    def doc(self, doc: str):
        self._doc = doc
        return self

    def member(self, _func_name: str, **args):
        tb = self._member.get(_func_name)
        if not tb:
            tb = {}
            self._member[_func_name] = tb
        f = _function_t(**args)
        key = _gen_args_key(**args)
        if tb.get(key):
            log_err(
                f"member {_func_name} already exists with same arguments overload.")
        tb[key] = f
        return f


def struct(_func_name: str):
    return struct_t(_func_name)


def _check_args(**args):
    for k in args:
        tp = args[k]
        if (tp in _basic_types) or (type(tp) == struct_t) or (type(tp) in _template_types):
            continue
        log_err(f"argument \"{k}\" use invalid type {str(tp)}")


def _check_ret_type(ret_type):
    if (ret_type in _basic_types) or (type(ret_type) == struct_t) or (ret_type == void) or (type(ret_type) in _template_types):
        return
    log_err(f"return type \"{str(ret_type)}\" with invalid")

# {
#     name: str,
#     func: function_t
# }


def register_global_function(_func_name: str, **args):
    func = _function_t(**args)
    tb = _registed_global_funcs.get(_func_name)
    if not tb:
        tb = {}
        _registed_global_funcs[_func_name] = tb
    key = _gen_args_key(**args)
    if tb.get(key):
        log_err(
            f"Function {_func_name} already exists with same arguments overload.")
    tb[key] = func
    return func
