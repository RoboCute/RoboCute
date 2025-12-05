import rbc_meta.utils.type_register_base as base

# 导入新版架构
try:
    import rbc_meta.utils_next.reflect as reflect_new
    import rbc_meta.utils_next.generator as generator_new

    _USE_NEW_ARCH = True
except ImportError:
    _USE_NEW_ARCH = False


class VoidPtr:
    pass


class DataBuffer:
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


class external_type:
    def __init__(self, name: str):
        self._name = name


class GUID:
    pass


class bool:
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


_basic_types = {
    bool,
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
    DataBuffer,
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
    GUID,
}

_template_types = {ClassPtr, unordered_map, external_type}
_registed_struct_types = {}
_registed_enum_types = {}


class enum:
    def __init__(self, _func_name: str, _enable_serde: bool = True, **params):
        namespace_cut = _func_name.rfind("::")

        self._serde = _enable_serde
        self._cpp_external = False
        self._py_external = False

        if namespace_cut >= 0:
            self._namespace_name = _func_name[0:namespace_cut]
            self._class_name = _func_name[namespace_cut + 2 : len(_func_name)]
        else:
            self._namespace_name = ""
            self._class_name = _func_name
        full_name = self.full_name()
        if _registed_enum_types.get(full_name):
            log_err(f"enum {full_name} already exists.")
        _registed_enum_types[full_name] = self
        self._params = []
        for i in params:
            self._params.append((i, params[i]))

        # 使用新版架构注册（如果可用）
        if _USE_NEW_ARCH:
            self._register_to_new_arch()

    def _register_to_new_arch(self):
        """将枚举注册到新版架构"""
        try:
            from enum import Enum as PyEnum

            # 创建枚举字典
            enum_dict = {}
            for name, value in self._params:
                enum_dict[name] = value if value is not None else name

            # 动态创建枚举类
            enum_cls = PyEnum(self.class_name(), enum_dict)
            # 注册到新版架构
            registry = reflect_new.ReflectionRegistry()
            module_name = (
                self._namespace_name.replace("::", ".")
                if self._namespace_name
                else __name__
            )
            registry.register(enum_cls, module_name)
            self._new_arch_class = enum_cls
        except Exception as e:
            # 如果注册失败，继续使用旧版架构
            pass

    def add(self, key: str, value=None):
        self._params.append((key, value))

    def namespace_name(self):
        return self._namespace_name

    def class_name(self):
        return self._class_name

    def full_name(self):
        s = self._namespace_name
        if len(s) > 0:
            s += "::"
        return s + self._class_name

    # will not generate this class in cpp
    def mark_cpp_external(self):
        self._cpp_external = True

    # will not generate this class in py
    def mark_py_external(self):
        self._py_external = True


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
        self._static = None

    def ret_type(self, ret_type):
        _check_ret_type(ret_type)
        self._ret_type = ret_type
        return self


class struct:
    def __init__(self, _func_name: str, suffix: str = ""):
        namespace_cut = _func_name.rfind("::")
        if namespace_cut >= 0:
            self._namespace_name = _func_name[0:namespace_cut]
            self._class_name = _func_name[namespace_cut + 2 : len(_func_name)]
        else:
            self._namespace_name = ""
            self._class_name = _func_name
        full_name = self.full_name()
        if _registed_struct_types.get(full_name):
            log_err(f"Struct {full_name} already exists.")
        _registed_struct_types[full_name] = self
        self._doc = full_name
        self._suffix = suffix
        self._members = dict()
        self._cpp_initer = dict()
        self._serde_members = dict()
        self._rpc = dict()
        self._methods = dict()
        self._cpp_external = False
        self._py_external = False
        self._new_arch_class = None

        # 使用新版架构注册（如果可用）
        if _USE_NEW_ARCH:
            # 延迟注册，等方法和成员都定义好后再注册
            pass

    def _register_to_new_arch(self):
        """将结构注册到新版架构（延迟注册，在方法和成员定义完成后调用）"""
        if not _USE_NEW_ARCH or self._new_arch_class is not None:
            return

        try:
            import inspect
            from typing import get_type_hints

            # 创建一个动态类来注册
            # 注意：由于旧版API使用类型对象而不是类型注解，我们需要创建一个适配层
            # 这里我们创建一个占位类，新版架构主要用于代码生成辅助

            # 创建类字典
            class_dict = {
                "__module__": __name__,
                "__doc__": self._doc,
            }

            # 添加字段（如果有）
            if self._members:
                # 为每个成员创建类型注解
                annotations = {}
                for name, type_obj in self._members.items():
                    # 将旧版类型对象映射到Python类型
                    annotations[name] = self._map_old_type_to_new(type_obj)
                class_dict["__annotations__"] = annotations

            # 创建动态类
            dynamic_cls = type(self._class_name, (object,), class_dict)

            # 注册到新版架构
            registry = reflect_new.ReflectionRegistry()
            module_name = (
                self._namespace_name.replace("::", ".")
                if self._namespace_name
                else __name__
            )
            registry.register(dynamic_cls, module_name)
            self._new_arch_class = dynamic_cls
        except Exception as e:
            # 如果注册失败，继续使用旧版架构
            pass

    def _map_old_type_to_new(self, old_type):
        """将旧版类型对象映射到Python类型"""
        # 基本类型映射
        type_mapping = {
            bool: bool,
            int: int,
            float: float,
            str: str,
            string: str,
            uint: int,
            int2: tuple,
            int3: tuple,
            int4: tuple,
            uint2: tuple,
            uint3: tuple,
            uint4: tuple,
            float2: tuple,
            float3: tuple,
            float4: tuple,
            VoidPtr: object,
            DataBuffer: bytes,
        }
        return type_mapping.get(old_type, object)

    def namespace_name(self):
        return self._namespace_name

    def class_name(self):
        return self._class_name

    def full_name(self):
        s = self._namespace_name
        if len(s) > 0:
            s += "::"
        return s + self._class_name

    def method(self, _func_name: str, **args):
        tb = self._methods.get(_func_name)

        if not tb:
            tb = {}
            self._methods[_func_name] = tb

        f = _function_t(**args)
        key = _gen_args_key(**args)
        if tb.get(key):
            log_err(f"member {_func_name} already exists with same arguments overload.")
        tb[key] = f

        # 尝试注册到新版架构
        if _USE_NEW_ARCH:
            try:
                self._register_to_new_arch()
            except:
                pass

        return f

    def rpc(self, _func_name: str, _is_static: bool, **args):
        tb = self._rpc.get(_func_name)
        if not tb:
            tb = {}
            self._rpc[_func_name] = tb
        f = _function_t(**args)
        f._static = _is_static
        key = _gen_args_key(**args)
        if tb.get(key):
            log_err(f"member {_func_name} already exists with same arguments overload.")
        tb[key] = f
        return f

    def members(self, **argv):
        for i in argv:
            v = argv[i]
            _check_arg(i, v)
            self._members[i] = v

        # 尝试注册到新版架构
        if _USE_NEW_ARCH:
            try:
                self._register_to_new_arch()
            except:
                pass

    def serde_members(self, **argv):
        for i in argv:
            v = argv[i]
            _check_arg(i, v)
            self._members[i] = v
            self._serde_members[i] = v

        # 尝试注册到新版架构
        if _USE_NEW_ARCH:
            try:
                self._register_to_new_arch()
            except:
                pass

    def init_member(self, cpp_initer):
        self._cpp_initer = cpp_initer

    # will not generate this class
    def mark_cpp_external(self):
        self._cpp_external = True

    # will not generate this class
    def mark_py_external(self):
        self._py_external = True


def _check_arg(key, arg):
    if (
        (arg in _basic_types)
        or (type(arg) == struct)
        or (type(arg) == enum)
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
        or (type(ret_type) == struct)
        or (ret_type == void)
        or (ret_type == enum)
        or (type(ret_type) in _template_types)
    ):
        return
    log_err(f'return type "{str(ret_type)}" with invalid')
