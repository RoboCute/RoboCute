# PySide6 + LuisaCompute 单进程嵌入 Demo

本目录包含一个最小 Demo，用于验证：**在单个 Python 进程内，外部 PySide6 主窗口能否嵌入 C++ Qt + LuisaCompute 的原生视口**。

> 范围：Demo 代码与交接文档，不实现真实功能。真实 UI 调试由用户接管。

## 目录结构

```text
samples/pyside_embed_demo/
├── __init__.py              # 包初始化与 Windows DLL 搜索路径注册
├── main_window.py           # PySide6 QMainWindow + LC ViewportWidget 嵌入
├── demo_app.py              # 命令行入口
├── run_demo.py              # 自动寻找扩展并启动 Demo 的脚本
├── validate_extension.py    # 非阻塞验证脚本（无需 PySide6/GUI）
├── HANDOVER.md              # 详细的交接文档、已知问题与调试 Checklist
└── README.md                # 本文档
```

## 快速开始

### 1. 非阻塞验证（当前环境即可运行）

```bash
xmake f -m releasedbg -c
xmake build --jobs=8 rbc_editor_py
uv run python samples/pyside_embed_demo/validate_extension.py
```

### 2. 运行真实 GUI Demo（需要 PySide6 环境）

```bash
uv run python samples/pyside_embed_demo/run_demo.py
```

或手动：

```bash
set PYTHONPATH=build\windows\x64\release;%CD%
uv run python -m samples.pyside_embed_demo.demo_app
```

## 重要限制

- 当前项目使用 **Python 3.14**，但 PySide6 目前仅提供到 **Python 3.13** 的 wheel。
  因此 GUI Demo 在当前 venv 无法运行，需要在 Python 3.12/3.13 环境中安装 PySide6 后测试。
- Demo 渲染器目前是 stub，仅用于验证窗口嵌入与事件转发；真实 LC 渲染需要替换为 `VisApp`/`RenderAppBase`。

详细说明见 [HANDOVER.md](HANDOVER.md)。
