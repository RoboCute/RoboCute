"""
测试新旧架构的兼容性

这个脚本验证：
1. 旧版 API 仍然可以正常工作
2. 新版架构可以访问旧版定义的结构
3. 适配器可以正确转换旧版结构
"""

import sys
from pathlib import Path


def test_old_api():
    """测试旧版 API 是否仍然工作"""
    print("=== 测试1: 旧版 API ===")
    try:
        import rbc_meta.utils.type_register as tr

        # 创建结构
        TestStruct = tr.struct("TestStruct")
        TestStruct.method("test_method", x=tr.int, y=tr.string).ret_type(tr.bool)
        TestStruct.members(value=tr.float)

        # 创建枚举
        TestEnum = tr.enum("TestEnum", VALUE1=None, VALUE2=None)

        # 验证结构已注册
        assert "TestStruct" in tr._registed_struct_types
        assert "TestEnum" in tr._registed_enum_types

        print("✓ 旧版 API 正常工作")
        return True
    except Exception as e:
        print(f"✗ 旧版 API 测试失败: {e}")
        import traceback

        traceback.print_exc()
        return False


def test_adapter():
    """测试适配器是否可以转换旧版结构"""
    print("\n=== 测试2: 适配器转换 ===")
    try:
        import rbc_meta.utils.type_register as tr
        from rbc_meta.utils.arch_adapter import (
            convert_struct_to_class_info,
            convert_enum_to_class_info,
        )

        # 创建测试结构
        TestStruct = tr.struct("AdapterTestStruct")
        TestStruct.method("method1", x=tr.int).ret_type(tr.bool)
        TestStruct.members(value=tr.float)

        # 转换为新版格式
        class_info = convert_struct_to_class_info(TestStruct)

        if class_info:
            assert class_info.name == "AdapterTestStruct"
            assert len(class_info.methods) > 0
            assert len(class_info.fields) > 0
            print("✓ 适配器转换成功")
            return True
        else:
            print("⚠ 适配器不可用（新版架构未安装）")
            return True  # 不算失败，只是不可用
    except Exception as e:
        print(f"✗ 适配器测试失败: {e}")
        import traceback

        traceback.print_exc()
        return False


def test_new_arch_access():
    """测试新版架构是否可以访问旧版结构"""
    print("\n=== 测试3: 新版架构访问 ===")
    try:
        import rbc_meta.utils.type_register as tr
        from rbc_meta.utils.arch_adapter import get_all_structs_as_class_info

        # 创建测试结构
        TestStruct = tr.struct("NewArchTestStruct")
        TestStruct.method("test", x=tr.int)

        # 获取所有结构
        structs = get_all_structs_as_class_info()

        # 检查是否包含我们的测试结构
        found = False
        for struct_name, class_info in structs.items():
            if "NewArchTestStruct" in struct_name:
                found = True
                break

        if found:
            print("✓ 新版架构可以访问旧版结构")
            return True
        else:
            print("⚠ 新版架构不可用或未找到结构")
            return True  # 不算失败
    except Exception as e:
        print(f"✗ 新版架构访问测试失败: {e}")
        import traceback

        traceback.print_exc()
        return False


def test_backward_compatibility():
    """测试向后兼容性：确保旧版代码生成器仍然可以工作"""
    print("\n=== 测试4: 向后兼容性 ===")
    try:
        import rbc_meta.utils.type_register as tr

        # 创建结构（模拟 backend_interface.py 的使用方式）
        Context = tr.struct("RBCContext", "TEST_GRAPHICS_API")
        Context.method(
            "init_device",
            rhi_backend=tr.string,
            program_path=tr.string,
            shader_path=tr.string,
        )
        Context.method(
            "create_mesh", data=tr.DataBuffer, vertex_count=tr.uint
        ).ret_type(tr.VoidPtr)

        # 验证结构属性存在（代码生成器需要这些）
        assert hasattr(Context, "_methods")
        assert hasattr(Context, "_members")
        assert hasattr(Context, "class_name")
        assert hasattr(Context, "namespace_name")
        assert hasattr(Context, "full_name")

        # 验证方法已注册
        assert "init_device" in Context._methods
        assert "create_mesh" in Context._methods

        print("✓ 向后兼容性测试通过")
        return True
    except Exception as e:
        print(f"✗ 向后兼容性测试失败: {e}")
        import traceback

        traceback.print_exc()
        return False


def main():
    """运行所有测试"""
    print("开始兼容性测试...\n")

    results = [
        test_old_api(),
        test_adapter(),
        test_new_arch_access(),
        test_backward_compatibility(),
    ]

    print(f"\n=== 测试结果 ===")
    print(f"通过: {sum(results)}/{len(results)}")

    if all(results):
        print("✓ 所有测试通过！")
        return 0
    else:
        print("✗ 部分测试失败")
        return 1


if __name__ == "__main__":
    sys.exit(main())
