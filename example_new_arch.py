"""
示例：如何使用新版架构来简化代码生成

这个文件展示了如何使用新版架构来生成代码，同时保持与旧版 API 的兼容性。
"""

import rbc_meta.utils.type_register as tr
from rbc_meta.utils.arch_adapter import (
    convert_struct_to_class_info,
    convert_enum_to_class_info,
    generate_code_with_new_arch,
    get_all_structs_as_class_info,
    get_all_enums_as_class_info,
)


# 示例1：使用旧版 API 定义结构（保持向后兼容）
def example_old_api():
    """使用旧版 API 定义结构"""
    MyStruct = tr.struct("MyStruct")
    MyStruct.method("do_something", x=tr.int, y=tr.string).ret_type(tr.bool)
    MyStruct.members(value=tr.float)

    # 旧版代码生成器仍然可以正常工作
    # ... 使用 codegen.py 中的函数生成代码 ...

    # 现在可以使用新版架构来生成代码
    try:
        python_code = generate_code_with_new_arch(
            struct_name="MyStruct", output_format="python"
        )
        print("生成的 Python 代码:")
        print(python_code)

        cpp_code = generate_code_with_new_arch(
            struct_name="MyStruct", output_format="cpp"
        )
        print("\n生成的 C++ 代码:")
        print(cpp_code)
    except Exception as e:
        print(f"新版架构不可用: {e}")


# 示例2：直接使用新版架构（推荐的新方式）
def example_new_api():
    """使用新版架构直接定义类"""
    from rbc_meta.utils_next import reflect, CodeGenerator

    @reflect
    class MyNewClass:
        """使用类型注解定义的新类"""

        value: float = 0.0

        def do_something(self, x: int, y: str) -> bool:
            """执行某些操作"""
            return True

    # 使用新版代码生成器
    generator = CodeGenerator()
    python_code = generator.generate_python("MyNewClass")
    print("生成的 Python 代码:")
    print(python_code)

    cpp_code = generator.generate_cpp_header("MyNewClass", namespace="myns")
    print("\n生成的 C++ 代码:")
    print(cpp_code)


# 示例3：混合使用（旧版 API + 新版代码生成）
def example_mixed():
    """混合使用旧版 API 和新版代码生成"""
    # 使用旧版 API 定义（因为需要特定的 C++ 类型映射）
    BackendContext = tr.struct("BackendContext", "RBC_API")
    BackendContext.method("init", backend=tr.string).ret_type(tr.bool)
    BackendContext.method("render", width=tr.uint, height=tr.uint)

    # 转换为新版格式并生成代码
    class_info = convert_struct_to_class_info(BackendContext)
    if class_info:
        from rbc_meta.utils_next.generator import PythonGenerator, CppGenerator

        # 生成 Python 代码
        python_code = PythonGenerator.generate_class(class_info)
        print("Python 接口:")
        print(python_code)

        # 生成 C++ 代码
        cpp_code = CppGenerator.generate_header(class_info, namespace="rbc")
        print("\nC++ 接口:")
        print(cpp_code)


# 示例4：批量转换所有结构
def example_batch_convert():
    """批量转换所有已注册的结构"""
    structs = get_all_structs_as_class_info()
    enums = get_all_enums_as_class_info()

    print(f"找到 {len(structs)} 个结构，{len(enums)} 个枚举")

    # 使用新版代码生成器生成所有代码
    from rbc_meta.utils_next.generator import CodeGenerator

    generator = CodeGenerator()

    for struct_name, class_info in structs.items():
        print(f"\n处理结构: {struct_name}")
        # 可以在这里生成代码或进行其他操作
        pass


if __name__ == "__main__":
    print("=== 示例1: 旧版 API ===")
    example_old_api()

    print("\n=== 示例2: 新版 API ===")
    example_new_api()

    print("\n=== 示例3: 混合使用 ===")
    example_mixed()

    print("\n=== 示例4: 批量转换 ===")
    example_batch_convert()
