import os
from pathlib import Path
# basic types


class VoidPtr:
    pass


class ClassPtr:
    def __init__(self, element=None):
        if element:
            _check_arg("vector element", element)
            self._element = element
        else:
            self._element = None


class unordered_map:
    def __init__(self, key, value):
        _check_args(key=key, value=value)
        self._key = key
        self._value = value


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
        _check_arg("vector element", element)
        self._element = element


_basic_types = {
    byte,
    short,
    int,
    long,
    ubyte,
    ushort,
    uint,
    ulong,
    float,
    double,
    string,
    int2,
    int3,
    int4,
    uint2,
    uint3,
    uint4,
    long2,
    long3,
    VoidPtr,
    long4,
    ulong2,
    ulong3,
    ulong4,
    float2,
    float3,
    float4,
    double2,
    double3,
    double4,
    float2x2,
    float3x3,
    float4x4,
    double2x2,
    double3x3,
    double4x4,
}
_template_types = {vector, ClassPtr, unordered_map}
_registed_struct_types = {}


def log_err(s: str):
    print(f"\033[31m{s}\033[0m")
    exit(1)


def _gen_args_key(**args):
    key = ""
    for i in args:
        key += str(args[i]) + "|"
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
        namespace_cut = _func_name.rfind('::')
        if namespace_cut >= 0:
            self._namespace_name = _func_name[0:namespace_cut]
            self._class_name = _func_name[namespace_cut+2:len(_func_name)]
        else:
            self._namespace_name = ''
            self._class_name = _func_name
        full_name = self.full_name()
        if _registed_struct_types.get(full_name):
            log_err(f"Struct {full_name} already exists.")
        _registed_struct_types[full_name] = self
        self._doc = full_name
        self._members = dict()
        self._methods = dict()

    def namespace_name(self):
        return self._namespace_name

    def class_name(self):
        return self._class_name

    def full_name(self):
        return self._namespace_name + self._class_name

    def doc(self, doc: str):
        self._doc = doc
        return self

    def method(self, _func_name: str, **args):
        tb = self._methods.get(_func_name)
        if not tb:
            tb = {}
            self._methods[_func_name] = tb
        f = _function_t(**args)
        key = _gen_args_key(**args)
        if tb.get(key):
            log_err(
                f"member {_func_name} already exists with same arguments overload.")
        tb[key] = f
        return f

    def members(self, **argv):
        for i in argv:
            v = argv[i]
            _check_arg(i, v)
            self._members[i] = v


def struct(_func_name: str):
    return struct_t(_func_name)


def _check_arg(key, arg):
    if (
        (arg in _basic_types)
        or (type(arg) == struct_t)
        or (type(arg) in _template_types)
    ):
        return
    log_err(f'argument "{key}" use invalid type {str(arg)}')


def _check_args(**args):
    for k in args:
        _check_arg(k, args[k])


def _check_ret_type(ret_type):
    if (
        (ret_type in _basic_types)
        or (type(ret_type) == struct_t)
        or (ret_type == void)
        or (type(ret_type) in _template_types)
    ):
        return
    log_err(f'return type "{str(ret_type)}" with invalid')
