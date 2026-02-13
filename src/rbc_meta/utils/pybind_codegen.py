from rbc_meta.utils.reflect import (
    ClassInfo,
    MethodInfo,
    FieldInfo,
)
from rbc_meta.utils.templates import (
    DEFAULT_INDENT,
    PYBIND_ENUM_BINDING_TEMPLATE,
    PYBIND_ENUM_VALUE_TEMPLATE)

def pybind_enum_binding(info: ClassInfo, INDENT: str = DEFAULT_INDENT):
    if not info.is_enum:
        return ""
    enum_name = info.name
    namespace_name = info.cpp_namespace or ""
    enum_values = "\n".join(
        [
            PYBIND_ENUM_VALUE_TEMPLATE.substitute(
                INDENT=INDENT,
                VALUE_NAME=field.name,
                ENUM_NAME=enum_name,  # Use full key for enum name
            )
            for field in info.fields
        ]
    )
    return PYBIND_ENUM_BINDING_TEMPLATE.substitute(
        INDENT=INDENT,
        NAMESPACE_NAME=namespace_name,
        ENUM_NAME=enum_name,
        CLASS_NAME=info.name,
        ENUM_VALUES=enum_values,
    )
