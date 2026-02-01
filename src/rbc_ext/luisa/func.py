try:
    import sourceinspect
except ImportError:
    print('sourceinspect not installed. This may cause issues in interactive mode (REPL).')
    import inspect as sourceinspect
    # need sourceinspect for getting source. see (#10)
import inspect
import ast

from rbc_ext._C import test_py_codegen as lcapi
from . import globalvars, astbuilder
from .builtin import builtin_func_names
from .globalvars import get_global_device
from .types import dtype_of, to_lctype, implicit_convertible, basic_dtypes, uint
from .astbuilder import VariableInfo
import textwrap
from pathlib import Path
import sys


def create_arg_expr(dtype, allow_ref):
    # Note: scalars are always passed by value
    #       vectors/matrices/arrays/structs are passed by reference if (allow_ref==True)
    #       resources are always passed by reference (without specifying ref)
    lctype = to_lctype(dtype)  # also checking that it's valid data dtype
    if lctype.is_scalar() or lctype.is_vector() or lctype.is_matrix():
        return lcapi.builder().argument(lctype)
    elif lctype.is_array() or lctype.is_structure():
        if allow_ref:
            return lcapi.builder().reference(lctype)
        else:
            return lcapi.builder().argument(lctype)
    elif lctype.is_buffer() or lctype.is_custom_buffer():
        return lcapi.builder().buffer(lctype)
    elif lctype.is_texture():
        return lcapi.builder().texture(lctype)
    else:
        assert False


# annotation can be used (but not required) to specify argument type
def annotation_type_check(funcname, parameters, argtypes):
    def anno_str(anno):
        if anno == inspect._empty:
            return ""
        if hasattr(anno, '__name__'):
            return ":" + anno.__name__
        return ":" + repr(anno)

    count = len(argtypes)
    for idx, name in enumerate(parameters):
        if idx >= count:
            break
        anno = parameters[name].annotation
        if anno != inspect._empty and not implicit_convertible(anno, argtypes[idx]):
            hint = funcname + '(' + ', '.join([n + anno_str(parameters[n].annotation) for n in parameters]) + ')'
            raise TypeError(f"argument '{name}' expects {anno}, got {argtypes[idx]}. calling {hint}")


# variables, information and compiled result are stored per func instance (argument type specialization)
class FuncInstanceInfo:
    def __init__(self, func, call_from_host, argtypes):
        self.func = func
        self.__name__ = func.__name__
        self.sourcelines = func.sourcelines
        # self.return_type is not defined until a return statement is met
        self.call_from_host = call_from_host
        self.argtypes = argtypes
        _closure_vars = inspect.getclosurevars(func.pyfunc)
        self.closure_variable = {
            **_closure_vars.globals,
            **_closure_vars.nonlocals,
            **_closure_vars.builtins
        }
        self.local_variable = {}  # dict: name -> VariableInfo(dtype, expr, is_arg)
        self.default_arg_values = self._capture_default_arg_values(func.pyfunc)
        self.function = None
        self.shader_handle = None

    def __del__(self):
        if self.shader_handle is not None:
            device = get_global_device()
            if device is not None:
                device.destroy_shader(self.shader_handle)

    def _capture_default_arg_values(self, pyfunc):
        sig = inspect.signature(pyfunc)
        return {param.name: param.default for param in sig.parameters.values()
                if param.default is not inspect.Parameter.empty}
    
    def build_arguments(self, allow_ref: bool, arg_info=None):
        if arg_info is None:
            for idx, name in enumerate(self.func.parameters):
                if idx >= len(self.argtypes): break
                dtype = self.argtypes[idx]
                expr = create_arg_expr(dtype, allow_ref=allow_ref)
                self.local_variable[name] = VariableInfo(dtype, expr, is_arg=True)
        else:
            for idx, name in enumerate(self.func.parameters):
                if idx >= len(self.argtypes): break
                var_info = arg_info.get(idx)
                dtype = self.argtypes[idx]
                if var_info is not None:
                    self.local_variable[name] = var_info["var"]
                else:
                    expr = create_arg_expr(dtype, allow_ref=allow_ref)
                    self.local_variable[name] = VariableInfo(dtype, expr, is_arg=True)


class CompileError(Exception):
    pass

type_idx = 0
class func:
    # creates a luisa function with given function
    # A luisa function can be run on accelarated device (CPU/GPU).
    # It can either be called in parallel by python code,
    # or be called by another luisa function.
    # pyfunc: python function
    def __init__(self, pyfunc):
        self.fence_idx = -1
        self.pyfunc = pyfunc
        self.__name__ = pyfunc.__name__
        if self.__name__ in builtin_func_names:
            raise RuntimeError(f'Cannot override builtin function "{self.__name__}"')
        self.compiled_results = {}  # maps (arg_type_tuple) to (function, shader_handle)
        frameinfo = inspect.getframeinfo(inspect.stack()[1][0])
        self.filename = frameinfo.filename
        self.lineno = frameinfo.lineno
    # compiles an argument-type-specialized callable/kernel
    # returns FuncInstanceInfo
    def compile(self, func_type: int, allow_ref: bool, argtypes: tuple, arg_info=None):
        call_from_host = func_type == 0
        # get python AST & context
        self.sourcelines = sourceinspect.getsourcelines(self.pyfunc)[0]
        uses_autodiff = "autodiff():" in "".join(self.sourcelines)
        self.sourcelines = [textwrap.fill(line, tabsize=4, width=9999) for line in self.sourcelines]
        self.tree = ast.parse(textwrap.dedent("\n".join(self.sourcelines)))
        self.parameters = inspect.signature(self.pyfunc).parameters
        if len(argtypes) > len(self.parameters):
            raise Exception(
                f"calling {self.__name__} with {len(argtypes)} arguments ({len(self.parameters)} or less expected).")
        # Check for too few arguments (considering default values)
        min_required_args = sum(1 for p in self.parameters.values() if p.default is inspect.Parameter.empty)
        if len(argtypes) < min_required_args:
            raise Exception(
                f"calling {self.__name__} with {len(argtypes)} arguments ({min_required_args} or more expected).")
        annotation_type_check(self.__name__, self.parameters, argtypes)
        f = FuncInstanceInfo(self, call_from_host, argtypes)

        # build function callback
        def astgen():
            if call_from_host:
                lcapi.builder().set_block_size(128, 1, 1)
            f.build_arguments(allow_ref=allow_ref, arg_info=arg_info)
            # push context & build function body AST
            top = globalvars.current_context
            globalvars.current_context = f
            try:
                astbuilder.build(self.tree.body[0])
            finally:
                globalvars.current_context = top

        # build function
        # Note: must retain the builder object
        match func_type:
            case 0:
                f.builder = lcapi.FunctionBuilder.define_kernel(astgen)
            case 1:
                f.builder = lcapi.FunctionBuilder.define_callable(astgen)
            case 2:
                f.builder = lcapi.FunctionBuilder.define_raster_stage(astgen)
        f.function = f.builder.function()
        # compile shader
        if call_from_host:
            globalvars.saved_shader_count += 1
            f.shader_handle = get_global_device().create_shader(f.function)
        return f

    # looks up arg_type_tuple; compile if not existing
    # returns FuncInstanceInfo
    def get_compiled(self, func_type: int, allow_ref: bool, argtypes: tuple, arg_info=None, custom_key=None):
        if custom_key != None and self.fence_idx < custom_key:
            self.fence_idx = custom_key
            self.compiled_results.clear()

        arg_features = (func_type,) + argtypes
        if arg_features not in self.compiled_results:
            try:
                self.compiled_results[arg_features] = self.compile(func_type, allow_ref, argtypes, arg_info)
            except Exception as e:
                if hasattr(e, "already_printed"):
                    # hide the verbose traceback in AST builder
                    e1 = CompileError(f"Failed to compile luisa.func '{self.__name__}'")
                    e1.func = self
                    raise e1 from None
                else:
                    raise
        return self.compiled_results[arg_features]

    # dispatch shader to stream
    def __call__(self, *args, dispatch_size=None, stream=None, dispatch_buffer_offset:int=0, max_dispatch_size:int=(2**32-1)):
        get_global_device()  # check device is initialized
        if stream is None:
            stream = globalvars.device
        # get 3D dispatch size
        is_buffer = False
        if type(dispatch_size) is int:
            dispatch_size = (dispatch_size, 1, 1)
        elif (type(dispatch_size) == tuple or type(dispatch_size) == list) and (len(dispatch_size) in (1, 2, 3)):
            dispatch_size = (*dispatch_size, *[1] * (3 - len(dispatch_size)))
        else:
            is_buffer = True
        # get types of arguments and compile
        argtypes = tuple(dtype_of(a) for a in args)
        f = self.get_compiled(func_type=0, allow_ref=False, argtypes=argtypes)
        # create command
        command = lcapi.ComputeDispatchCmdEncoder.create(f.function.argument_size(), f.shader_handle, f.function)
        # push arguments
        for a in args:
            lctype = to_lctype(dtype_of(a))
            if lctype.is_basic():
                command.encode_uniform(lcapi.to_bytes(a), lctype.size(), lctype.alignment())
            elif lctype.is_array() or lctype.is_structure():
                command.encode_uniform(a.to_bytes(), lctype.size(), lctype.alignment())
            elif lctype.is_buffer() or lctype.is_custom_buffer():
                command.encode_buffer(a.handle, 0, a.bytesize)
            elif lctype.is_texture():
                command.encode_texture(a.handle, 0)
            else:
                assert False
        # dispatch
        if is_buffer:
            command.set_dispatch_buffer(dispatch_size.handle, dispatch_buffer_offset, max_dispatch_size)
        else:
            command.set_dispatch_size(*dispatch_size)
        stream.add(command.build())
