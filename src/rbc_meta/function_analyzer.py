#!/usr/bin/env python3
"""
Python 函数统计工具
功能：分析 Python 文件或项目中的所有函数定义，生成详细统计报告
"""

import ast
import os
import sys
import argparse
from pathlib import Path
from collections import defaultdict
from dataclasses import dataclass, field
from typing import List, Dict, Optional


@dataclass
class FunctionInfo:
    """存储函数信息的数据类"""
    name: str
    line_number: int
    col_offset: int
    args: List[str]
    decorators: List[str]
    is_method: bool = False
    is_async: bool = False
    is_generator: bool = False
    class_name: Optional[str] = None
    complexity: int = 1
    docstring: Optional[str] = None
    return_annotation: Optional[str] = None
    file_path: str = ""  # 用于目录分析时记录来源文件


class PythonFunctionAnalyzer(ast.NodeVisitor):
    """AST 访问者，用于分析 Python 文件中的函数"""

    def __init__(self):
        self.functions: List[FunctionInfo] = []
        self.current_class: Optional[str] = None

    def visit_ClassDef(self, node):
        """访问类定义"""
        old_class = self.current_class
        self.current_class = node.name

        for item in node.body:
            if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)):
                self.visit(item)

        self.current_class = old_class

    def visit_FunctionDef(self, node):
        """访问普通函数定义"""
        self._process_function(node, is_async=False)
        self.generic_visit(node)

    def visit_AsyncFunctionDef(self, node):
        """访问异步函数定义"""
        self._process_function(node, is_async=True)
        self.generic_visit(node)

    def _process_function(self, node, is_async: bool):
        """处理函数节点"""
        args = []
        for arg in node.args.args:
            arg_name = arg.arg
            if arg.annotation:
                try:
                    args.append(f"{arg_name}: {ast.unparse(arg.annotation)}")
                except:
                    args.append(arg_name)
            else:
                args.append(arg_name)

        decorators = []
        for decorator in node.decorator_list:
            if isinstance(decorator, ast.Name):
                decorators.append(decorator.id)
            elif isinstance(decorator, ast.Call):
                if isinstance(decorator.func, ast.Name):
                    decorators.append(f"{decorator.func.id}()")
            elif isinstance(decorator, ast.Attribute):
                try:
                    decorators.append(ast.unparse(decorator))
                except:
                    decorators.append("decorator")

        is_generator = any(
            isinstance(child, (ast.Yield, ast.YieldFrom))
            for child in ast.walk(node)
        )

        docstring = ast.get_docstring(node)

        return_annotation = None
        if node.returns:
            try:
                return_annotation = ast.unparse(node.returns)
            except:
                pass

        complexity = 1
        for child in ast.walk(node):
            if isinstance(child, (ast.If, ast.While, ast.For,
                                  ast.ExceptHandler, ast.With,
                                  ast.Assert, ast.comprehension)):
                complexity += 1
            elif isinstance(child, ast.BoolOp):
                complexity += len(child.values) - 1

        func_info = FunctionInfo(
            name=node.name,
            line_number=node.lineno,
            col_offset=node.col_offset,
            args=args,
            decorators=decorators,
            is_method=self.current_class is not None,
            is_async=is_async,
            is_generator=is_generator,
            class_name=self.current_class,
            complexity=complexity,
            docstring=docstring,
            return_annotation=return_annotation
        )

        self.functions.append(func_info)


def analyze_python_file(file_path: str) -> Dict:
    """分析单个 Python 文件"""
    if not os.path.exists(file_path):
        return {"error": f"文件不存在: {file_path}"}

    if not file_path.endswith('.py'):
        return {"error": "不是 Python 文件"}

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            source = f.read()
    except Exception as e:
        return {"error": f"读取文件失败: {str(e)}"}

    try:
        tree = ast.parse(source)
    except SyntaxError as e:
        return {"error": f"语法错误: {str(e)}"}

    analyzer = PythonFunctionAnalyzer()
    analyzer.visit(tree)

    functions = analyzer.functions

    stats = {
        "file_path": file_path,
        "total_lines": len(source.splitlines()),
        "total_functions": len(functions),
        "regular_functions": len([f for f in functions if not f.is_method]),
        "methods": len([f for f in functions if f.is_method]),
        "async_functions": len([f for f in functions if f.is_async]),
        "generators": len([f for f in functions if f.is_generator]),
        "functions_with_docstring": len([f for f in functions if f.docstring]),
        "average_complexity": sum(f.complexity for f in functions) / len(functions) if functions else 0,
        "max_complexity": max((f.complexity for f in functions), default=0),
        "functions": functions
    }

    return stats


def main():
    parser = argparse.ArgumentParser(
        description='统计 Python 文件或项目中的所有函数',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  %(prog)s my_script.py              # 分析单个文件
  %(prog)s ./my_project -r           # 递归分析项目
  %(prog)s ./src -t 10               # 显示前10个最复杂的函数
  %(prog)s ./project --exclude tests # 排除测试目录
        """
    )

    parser.add_argument('path', help='Python 文件或目录路径')
    parser.add_argument('-o', '--out', type=str,
                        help='输出目录')
    parser.add_argument('-t', '--template', type=str,
                        help='文本替换模板')
    args = parser.parse_args()

    path = Path(args.path)
    template_str = '$'

    if not path.exists():
        print(f"❌ 错误: 路径不存在 {args.path}")
        sys.exit(1)

    if args.template:
        # 分析单个文件
        try:
            with open(args.template, 'r', encoding='utf-8') as f:
                template_str = f.read()
        except Exception as e:
            print(f"❌ 错误: 无法读取模板文件: {str(e)}")
            sys.exit(1)

    if not path.exists():
        print(f"❌ 错误: 路径不存在 {args.path}")
        sys.exit(1)

    if path.is_file():
        # 分析单个文件
        stats = analyze_python_file(str(path))
    else:
        raise Exception('Directory not supported.')

    if args.out:
        out_dir = Path(args.out)
        out_dir.mkdir(parents=True, exist_ok=True)
        if "error" not in stats:
            file_name = path.stem + '_functions.txt'
            out_file = out_dir / file_name
            print(out_file)
            with open(out_file, 'w', encoding='utf-8') as f:
                for func in stats.get('functions', []):
                    name: str = func.name
                    if func.class_name is not None or name.startswith('_'):
                        continue
                    s = template_str.replace('$', name)
                    f.write(s + '\n')
            print(f"✅ 函数名列表已保存到: {out_file}")


if __name__ == '__main__':
    main()
