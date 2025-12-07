"""
测试reflect模块的泛型支持功能
演示如何收集和处理泛型容器类型
"""

from typing import List, Dict, Optional
from rbc_meta.utils_next import reflect, ReflectionRegistry, GenericInfo


# 测试类：包含各种泛型字段
@reflect
class TestGenericClass:
    """测试泛型支持的类"""

    # List泛型
    int_list: List[int]
    str_list: List[str]

    # Dict泛型
    str_int_map: Dict[str, int]
    int_float_map: Dict[int, float]

    # Optional泛型
    optional_int: Optional[int]
    optional_str: Optional[str]

    # 普通类型
    normal_int: int
    normal_str: str

    def process_list(self, items: List[int]) -> List[str]:
        """处理列表的方法"""
        return []

    def process_dict(self, data: Dict[str, int]) -> Optional[Dict[int, str]]:
        """处理字典的方法"""
        return None


def test_generic_collection():
    """测试泛型收集功能"""
    registry = ReflectionRegistry()
    registry.register(TestGenericClass)

    # 获取类信息
    class_info = registry.get_class_info("TestGenericClass")
    if class_info is None:
        print("错误：无法获取类信息")
        return

    print(f"类名: {class_info.name}")
    print(f"\n字段信息:")
    print("-" * 60)

    for field in class_info.fields:
        print(f"字段名: {field.name}")
        print(f"  类型: {field.type}")
        if field.generic_info:
            print(f"  泛型信息:")
            print(f"    原始类型: {field.generic_info.origin}")
            print(f"    泛型参数: {field.generic_info.args}")
            print(f"    C++类型名: {field.generic_info.cpp_name}")
            print(f"    是否为容器: {field.generic_info.is_container}")
            print(f"    是否为Optional: {field.generic_info.is_optional}")
        else:
            print(f"  非泛型类型")
        print()

    print(f"\n方法信息:")
    print("-" * 60)

    for method in class_info.methods:
        print(f"方法名: {method.name}")
        print(f"  返回类型: {method.return_type}")
        if method.return_type_generic:
            print(f"  返回类型泛型信息:")
            print(f"    原始类型: {method.return_type_generic.origin}")
            print(f"    泛型参数: {method.return_type_generic.args}")
            print(f"    C++类型名: {method.return_type_generic.cpp_name}")
            print(f"    是否为容器: {method.return_type_generic.is_container}")
            print(f"    是否为Optional: {method.return_type_generic.is_optional}")

        if method.parameter_generics:
            print(f"  参数泛型信息:")
            for param_name, param_generic in method.parameter_generics.items():
                if param_generic:
                    print(f"    {param_name}:")
                    print(f"      原始类型: {param_generic.origin}")
                    print(f"      泛型参数: {param_generic.args}")
                    print(f"      C++类型名: {param_generic.cpp_name}")
                    print(f"      是否为容器: {param_generic.is_container}")
        print()


def test_builtin_generic_types():
    """测试内置泛型类型"""
    from rbc_meta.utils_next.builtin import Vector

    registry = ReflectionRegistry()
    registry.register(Vector)

    class_info = registry.get_class_info("Vector")
    if class_info:
        print(f"\n内置Vector类型信息:")
        print(f"  类名: {class_info.name}")
        print(f"  模块: {class_info.module}")
        print(f"  基类: {class_info.base_classes}")


if __name__ == "__main__":
    print("=" * 60)
    print("测试泛型收集功能")
    print("=" * 60)
    print()

    test_generic_collection()
    test_builtin_generic_types()

    print("\n测试完成！")
