---
name: coding
---

在这个项目中，请遵循以下规范：

- 使用 4 空格缩进
- 类名使用首字母大写 CamelCase
- 变量名使用 camelCase
- 函数名使用 snake_case
- 每个函数都需要 docstring
- 单行不超过 100 字符
- 调用 `xmake f -m debug -c`  初始化配置
- 调用 `xmake` 编译项目
- 调用 `xmake run <target_name>` 运行 target
- Python 函数必须写 Type Hints
- 使用 `uv run <file_path>` 运行测试 Python