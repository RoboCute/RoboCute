"""
Python 绑定代码生成实现
"""

from typing import Type, TYPE_CHECKING

from rbc_meta.utils.reflect import ReflectionRegistry
from rbc_meta.utils.templates import (
    DEFAULT_INDENT,
    PY_MODULE_TEMPLATE,
    PY_INTERFACE_CLASS_TEMPLATE,
    PY_METHOD_DISPOSE_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE,
    PY_INIT_METHOD_TEMPLATE_EXTERNAL,
    PY_DISPOSE_METHOD_TEMPLATE,
    PY_BOOL_METHOD_TEMPLATE,
    PY_METHOD_TEMPLATE,
    PY_MODULE_IMPORT_TEMPLATE,
    PYBIND_METHOD_NAME_TEMPLATE,
)
from rbc_meta.utils.codegen_util import _write_string_to
from rbc_meta.utils.pybind_codegen import (
    _print_py_args,
    _get_py_type,
    _print_py_args_decl,
)

if TYPE_CHECKING:
    from rbc_meta.utils.codegenx import CodeModule
    from rbc_meta.utils.reflect import ClassInfo, MethodInfo


def gen_pybind_py(mod: "CodeModule"):
    """生成 Python 绑定文件"""
    target_filepath = mod.pybind_py_file
    print("Generating pybind to ", target_filepath)
    registry = ReflectionRegistry()
    INDENT = DEFAULT_INDENT
    type_to_cls_info = {}

    def get_class_expr(info: "ClassInfo"):
        if info.is_enum:
            return "", []

        struct_name = info.name
        if not info.pybind or not info.create_instance:
            init_method = PY_INIT_METHOD_TEMPLATE_EXTERNAL.substitute(INDENT=INDENT)
            dispose_method = ""
        else:
            init_method = PY_INIT_METHOD_TEMPLATE.substitute(
                INDENT=INDENT,
                STRUCT_NAME=struct_name,
            )

            dispose_method = PY_DISPOSE_METHOD_TEMPLATE.substitute(INDENT=INDENT)
        dispose_method += PY_BOOL_METHOD_TEMPLATE.substitute(INDENT=INDENT)
        pybind_methods_list = []
        if info.create_instance:
            pybind_methods_list.append(f"create__{struct_name}__")

        def get_method_expr(method: "MethodInfo", type: Type):
            method_params = {
                k: v for k, v in method.parameters.items() if k != "self"
            }

            args_decl = _print_py_args_decl(method_params, False, type)
            args_call = _print_py_args(method_params, False, False)

            return_expr = "return " if method.return_type else ""
            return_end = ""
            if method.return_type:
                if hasattr(method.return_type, "_ctor_begin") or (
                    hasattr(method.return_type, "_pybind_type_")
                    and method.return_type._pybind_type_
                    and (
                        not hasattr(method.return_type, "_is_enum_")
                        or not method.return_type._is_enum_
                    )
                ):
                    return_expr += _get_py_type(method.return_type)
                    if (
                        hasattr(method.return_type, "_ctor_begin")
                        and method.return_type._ctor_begin
                    ):
                        return_expr += "."
                        return_expr += method.return_type._ctor_begin
                    else:
                        return_expr += "("
                    if (
                        hasattr(method.return_type, "_ctor_end")
                        and method.return_type._ctor_end
                    ):
                        return_end = method.return_type._ctor_end
                    else:
                        return_end += ")"

            pybind_method_name = PYBIND_METHOD_NAME_TEMPLATE.substitute(
                STRUCT_NAME=struct_name,
                METHOD_NAME=method.name,
            )

            pybind_methods_list.append(pybind_method_name)
            if method.name == "dispose":
                return PY_METHOD_DISPOSE_TEMPLATE.substitute(
                    INDENT=INDENT,
                    METHOD_NAME=method.name,
                    ARGS_DECL=args_decl,
                    RETURN_EXPR=return_expr,
                    PYBIND_METHOD_NAME=pybind_method_name,
                    ARGS_CALL=args_call,
                    RETURN_END=return_end,
                ), pybind_methods_list
            return PY_METHOD_TEMPLATE.substitute(
                INDENT=INDENT,
                METHOD_NAME=method.name,
                ARGS_DECL=args_decl,
                RETURN_EXPR=return_expr,
                PYBIND_METHOD_NAME=pybind_method_name,
                ARGS_CALL=args_call,
                RETURN_END=return_end,
            ), pybind_methods_list

        methods_list = []
        for method in info.methods:
            if method.is_inherit_func:
                continue
            method_expr, _ = get_method_expr(method, info.cls)
            methods_list.append(method_expr)

        methods_expr = "".join(methods_list)
        inherit_expr = ""
        if len(info.base_classes) == 1:
            base_class = info.base_classes[0]
            assert base_class is not None
            base_expr = _get_py_type(base_class.cls)
            inherit_expr = f"({base_expr})"
        elif len(info.base_classes) > 1:
            print(f"{info.name} has more than 1 base classes")

        return PY_INTERFACE_CLASS_TEMPLATE.substitute(
            CLASS_NAME=info.name,
            INHERIT_EXPR=inherit_expr,
            INIT_METHOD=init_method,
            DISPOSE_METHOD=dispose_method,
            METHODS_EXPR=methods_expr,
        ), pybind_methods_list

    classes_expr_list = []
    enum_exprs = []

    import_reqs = []
    import_cls_reqs = []
    for cls in mod.classes:
        info = registry.get_class_info(cls.__name__)
        type_to_cls_info[info.cls] = info
        if not info.pybind:
            continue

        if info.is_enum:
            import_cls_reqs.append(info.name)

        class_expr, pybind_methods_list = get_class_expr(info)
        if class_expr:
            classes_expr_list.append(class_expr)

        import_reqs.extend(pybind_methods_list)

    import_reqs_expr = "*"
    if len(import_reqs) > 0:
        import_reqs_expr = ",".join(import_reqs)
    import_cls_reqs_expr = "*"
    if len(import_cls_reqs) > 0:
        import_cls_reqs_expr = ",".join(import_cls_reqs)

    classes_expr = "\n".join(classes_expr_list)
    enum_exprs = "\n".join(enum_exprs)

    import_module_expr = PY_MODULE_IMPORT_TEMPLATE.substitute(
        MODE_NAME=mod.name,
        PYBIND_METHODS_EXPR=import_reqs_expr,
        PYBIND_CLS_EXPR=import_cls_reqs_expr,
    )

    file_expr = PY_MODULE_TEMPLATE.substitute(
        IMPORT_MODULE_EXPR=import_module_expr,
        ENUM_EXPRS=enum_exprs,
        CLASS_EXPRS=classes_expr,
    )
    _write_string_to(file_expr, mod.pybind_py_file)
