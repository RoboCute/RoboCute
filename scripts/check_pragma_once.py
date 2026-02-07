#!/usr/bin/env python3
"""
检查所有 .hpp 和 .h 文件的是否含有 #pragma once
"""

import os
from pathlib import Path


def check_file(filepath: Path) -> bool:
    """检查单个文件的是否含有 #pragma once"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            data = f.read()
            return data.find('#pragma once') != -1
    except Exception as e:
        print(f"无法读取文件 {filepath}: {e}")
        return False


def main():
    root_dir = Path('.')
    
    # 收集所有 .h 和 .hpp 文件
    header_files = list(root_dir.rglob('*.h')) + list(root_dir.rglob('*.hpp'))
    
    invalid_files = []
    
    for filepath in header_files:
        # 跳过隐藏目录下的文件
        if any(part.startswith('.') or part.startswith('thirdparty') or part.find('pch') != -1 for part in filepath.parts):
            continue
            
        if not check_file(filepath):
            invalid_files.append(filepath)
    
    if invalid_files:
        print("以下文件没有 '#pragma once':")
        for f in invalid_files:
            print(f"  {f}")
    
    return len(invalid_files)


if __name__ == '__main__':
    main()
    exit(0)
