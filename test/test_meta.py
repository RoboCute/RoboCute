import pytest
from rbc_meta.utils.codegenx import codegen, CodeModule, CodegenRegistry
from meta.meta_test_def import DummyMeta, BaseType, DerivedType


@codegen
class DummyModule(CodeModule):
    name = "dummy_module"
    cpp_interface_header = "test/meta/generated/include/dummy.hpp"
    cpp_impl_file = "test/meta/generated/src/dummy.cpp"
    pybind_py_file = "test/meta/generated/dummy.py"
    deps = []
    classes = [DummyMeta, BaseType, DerivedType]


def generate_registered():
    r = CodegenRegistry()
    r.generate()


def test_meta():
    generate_registered()
