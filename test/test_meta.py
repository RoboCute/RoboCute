import pytest
from rbc_meta.utils.codegenx import codegen, CodeModule, CodegenResitry
from meta.meta_test_def import DummyMeta, BaseType, DerivedType


@codegen
class DummyModule(CodeModule):
    cpp_base_dir_ = "test/meta/generated"
    enable_cpp_interface_ = True
    interface_header_file_ = "dummy.hpp"
    enable_cpp_impl_ = True
    cpp_impl_file_ = "dummy.cpp"
    enable_pybind_ = True
    pybind_py_file_ = "dummy.py"
    deps_ = []

    classes_ = [DummyMeta, BaseType, DerivedType]


def generate_registered():
    r = CodegenResitry()
    r.generate()


def test_meta():
    generate_registered()
