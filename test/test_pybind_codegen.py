from rbc_meta.utils.pybind_codegen import pybind_enum_binding
from rbc_meta.utils.reflect import (
    ReflectionRegistry,
    ClassInfo,
    MethodInfo,
    FieldInfo,
)
from meta.meta_test_def import DummyMeta, BaseType, DerivedType, DummyEnum


def test_pybind_enum_gen():
    registry = ReflectionRegistry()
    info = registry.get_class_info(DummyEnum.__name__)
    enum_binding = pybind_enum_binding(info)
    print(enum_binding)
    assert True