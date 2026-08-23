# PySide6 + LuisaCompute 单进程嵌入 Demo 交接文档

> 目标：在**单进程内**验证外部 PySide6 主窗口能否嵌入 C++ Qt + LuisaCompute 的渲染视口。
> 本次产出：**可编译的最小 Demo 代码 + 非阻塞验证脚本 + 本交接文档**。
> 真实 UI 运行与深度调试由用户接管。

---

## 1. 已验证能力

| 验证项 | 状态 | 说明 |
|---|---|---|
| C++ 扩展 `rbc_editor_py` 可编译为 `.pyd` | ✅ | 目标 `rbc/editor/python/` 已接入 xmake，build 成功。 |
| 扩展能在 Python 中导入并暴露 API | ✅ | `validate_extension.py` 通过：列出 8 个函数，`has_qapplication()` 返回 `False`（因为没有 QApp），`create_viewport()` 在缺少 QApp 时正确抛 `RuntimeError`。 |
| 扩展检测到 `QCoreApplication::instance()` | ✅ | `has_qapplication()` 复用了 `QCoreApplication::instance()`。 |
| Qt 6.9.3 与 rbc 构建使用同一 Qt | ✅ | `xmake f -c` 输出 `Qt SDK directory ... Qt/6.9.3/msvc2022_64`。 |
| 单进程架构代码已就绪 | ✅ | PySide6 `QMainWindow` + dock + `shiboken6.wrapInstance` 桥接 `ViewportWidget`。 |
| DLL 依赖加载路径 | ✅ | `validate_extension.py` 使用 `os.add_dll_directory` 加载成功。 |

---

## 2. 已知问题（必须在进入下一步前确认）

### 2.1 PySide6 与 Python 3.14 不兼容（关键阻塞）

当前项目 `requires-python = ">=3.14.3"`，而 **PySide6 目前最高仅支持 Python 3.13**。
直接 `uv add --dev PySide6` 会失败，因为没有 Python 3.14 的 wheel。

**影响**：
- `samples/pyside_embed_demo/main_window.py` 依赖 `PySide6`/`shiboken6`，无法在当前 venv 运行。
- `run_demo.py` / `demo_app.py` 不能真正启动 GUI。

**建议**：
- 在 Python 3.12 或 3.13 环境中安装 PySide6（如 `pip install PySide6==6.9.*`）。
- 重新配置 xmake（`xmake f -c`）使 `lc_py_include`/`lc_py_linkdir` 指向新的 Python 版本。
- 重新 build `rbc_editor_py` 目标，确保扩展与 PySide6 使用同一 Qt 版本与 Python ABI。

### 2.2 `rbc_editor` 构建选项默认关闭

`xmake/options.json` 中 `rbc_editor` 默认为 `false`，因此 editor 目标（含本 Demo 扩展）默认不会被加载。

**已做修改**：手动将 `xmake/options.json` 中的 `"rbc_editor"` 改为 `true` 并重新 `xmake f -c`。
注意：该文件由 `uv run prepare` 生成，重新运行 prepare 会覆盖它。

### 2.3 当前 Demo 渲染器是 Stub

`rbc/editor/python/src/demo_renderer.cpp` 实现了一个最小 `IRenderer`：
- `init()` 仅记录日志，不创建 LC device/stream。
- `process_qt_handle()` 为空。
- `get_present_texture()` 返回 `0`。

为配合 stub，已临时修改 `rbc/editor/runtime/src/ui/RHIWindow.cpp`：
- `ensureFullscreenTexture()`：当 `handle == 0` 时跳过纹理创建，避免 crash。
- `render()`：当 `_texture` 为空时只调用 `beginFrame`/`endFrame`，不画全屏 quad。

这意味着 Demo 启动后视口大概率是黑色，仅用于验证窗口嵌入与事件转发。

**下一步**：将 `DemoRenderer` 替换为 `rbc::VisApp`/`rbc::RenderAppBase`，并正确初始化 `GraphicsUtils` / `RenderDevice`，以恢复 LC 真实渲染。

### 2.4 `xmake/options.json` 中 Python 路径为 3.14.3，而 uv venv 使用 3.14.3

`options.json` 记录的 Python lib 来自 `uv run prepare` 期间探测到的解释器。
虽然验证通过，但建议统一 Python 版本，避免潜在的 ABI 微差异。

---

## 3. 文件清单

### 3.1 新增 / 修改的 C++ 文件

| 路径 | 作用 |
|---|---|
| `rbc/editor/python/xmake.lua` | 新增 xmake 目标 `rbc_editor_py`，输出 `.pyd`。 |
| `rbc/editor/python/src/zz_pch.h` | 预编译头，包含 pybind11 + Qt/LC 头。 |
| `rbc/editor/python/src/demo_renderer.h/.cpp` | Stub `IRenderer` 实现。 |
| `rbc/editor/python/src/rbc_editor_py.cpp` | pybind11 模块入口与 viewport 工厂函数。 |
| `rbc/editor/xmake.lua` | 增加 `includes("python")`。 |
| `rbc/editor/runtime/src/ui/RHIWindow.cpp` | 临时跳过 handle=0 时的全屏 quad 渲染（见 2.3）。 |
| `xmake/options.json` | 临时将 `rbc_editor` 改为 `true`（见 2.2）。 |

### 3.2 Python Demo 文件

| 路径 | 作用 |
|---|---|
| `samples/pyside_embed_demo/__init__.py` | 包初始化 + Windows DLL 搜索路径注册。 |
| `samples/pyside_embed_demo/main_window.py` | `MainWindow` + `LuisaViewportWidget`（shiboken 桥接）。 |
| `samples/pyside_embed_demo/demo_app.py` | 命令行入口。 |
| `samples/pyside_embed_demo/run_demo.py` | 自动寻找 `rbc_editor_py.pyd` 并启动 demo。 |
| `samples/pyside_embed_demo/validate_extension.py` | 非阻塞验证脚本，不创建任何窗口。 |
| `samples/pyside_embed_demo/HANDOVER.md` | 本文档。 |

---

## 4. 复现步骤

### 4.1 非阻塞验证（当前环境即可运行）

```powershell
# 1. 确保 xmake 已经配置了 editor 目标
xmake f -m releasedbg -c

# 2. 仅构建 demo 扩展
xmake build --jobs=8 rbc_editor_py

# 3. 运行验证脚本（不需要 PySide6）
uv run python samples/pyside_embed_demo/validate_extension.py
```

期望输出（节选）：

```text
  + has_qapplication
  + create_viewport
  ...
  has_qapplication() -> False
  create_viewport correctly raised RuntimeError: No QCoreApplication instance found...
  Validation passed.
```

### 4.2 运行真实 GUI Demo（需要 Python 3.12/3.13 + PySide6）

```powershell
# 在支持 PySide6 的 Python 环境中
uv sync --extra=all        # 或 pip install PySide6
uv run python samples/pyside_embed_demo/run_demo.py
```

如果不用 `run_demo.py`，手动设置环境：

```powershell
$env:PYTHONPATH = "build\windows\x64\release;${PWD}"
uv run python -m samples.pyside_embed_demo.demo_app
```

---

## 5. 调试 Checklist

### 5.1 Qt / PySide6 ABI 一致性

```python
# 在目标 Python 中执行
from PySide6.QtCore import qVersion
print(qVersion())          # 期望 6.9.3
import PySide6
print(PySide6.__version__) # 期望 6.9.x
```

同时确认 xmake 使用同一 Qt：

```bash
xmake show --qt=/path/to/Qt/6.9.3/msvc2022_64
```

### 5.2 QApplication 唯一性

```python
import rbc_editor_py
print(rbc_editor_py.has_qapplication())  # 创建 PySide6 QApplication 前应为 False
```

如果 `has_qapplication()` 在 `QApplication([])` 之前为 `True`，说明 C++ 扩展或某个依赖提前创建了 `QCoreApplication`，需要排查。

### 5.3 扩展能否找到依赖 DLL

若出现 `ImportError: DLL load failed`，检查：
- 是否已 `os.add_dll_directory` 加入：
  1. `build/windows/x64/release`（或实际 build 目录）
  2. `D:\tools\Qt\6.9.3\msvc2022_64\bin`
  3. 目标 Python 安装目录（含 `python3.dll` / `python314.dll`）
- 是否已 build `rbc_editor_runtime.dll`、`rbc_runtime.dll`、`luisa-*` 等（它们与 `rbc_editor_py.pyd` 同目录）。

### 5.4 视口是否正确嵌入

在 `main_window.py` 中 `self._wrapped = shiboken6.wrapInstance(...)` 之后打印：

```python
print(type(self._wrapped), self._wrapped.parent(), self._wrapped.size())
```

如果 `parent()` 为 `None` 或 `size()` 为 `(0, 0)`，说明跨语言父子关系/布局未生效，需要：
1. 在创建 C++ widget 前确保父 widget 已 `winId()`（已做）。
2. 尝试调用 `self._wrapped.setParent(self)` 后再 `show()`。
3. 备选方案：扩展不返回 `winId()`，而是直接返回 `QWidget*` 的 `PyCapsule` 让 PySide 用 `wrapInstance` 包装（需改 C++ API）。

### 5.5 事件透传

`ViewportWidget` 已经通过 `QCoreApplication::sendEvent` 将鼠标/键盘/滚轮转发到 `RhiWindow`。若事件不生效：
1. 点击视口后检查 `rbc_editor_py.viewport_info(wid)` 中 `last_key` 是否变化。
2. 在 `rbc/editor/runtime/src/ui/ViewportWidget.cpp` 的 `keyPressEvent` / `mousePressEvent` 中加 `qDebug()`。
3. 若 `RHIWindowContainerWidget` 导致焦点问题，可尝试把 `ViewportWidget` 的 parent 设为 central widget，而非额外 container。

### 5.6 退出时 GPU 资源释放

`ViewportWidget::~ViewportWidget()` 已调用 `releaseSwapChain()`。若退出崩溃：
1. 在 PySide `closeEvent` 中先调用 `rbc_editor_py.destroy_viewport(wid)`（已做）。
2. 确保 `destroy_viewport` 在 native window 仍有效时调用，不要拖到 `QApplication` 析构之后。
3. 检查 LC 渲染线程是否仍在运行；可在 `~DemoRenderer()` / 模块析构中停止它。

---

## 6. 下一步可尝试方向

1. **统一 Python 版本并安装 PySide6**
   - 使用 Python 3.12/3.13，确认 `PySide6.QtCore.qVersion() == "6.9.3"`。
   - 重新 `xmake f -c && xmake build --jobs=8 rbc_editor_py`。

2. **替换 Stub 渲染器为真实 LC 渲染**
   - 在 `rbc/editor/python/src/rbc_editor_py.cpp` 的 `create_viewport` 中初始化 `rbc::GraphicsUtils`。
   - 创建 `rbc::VisApp` 并调用 `visapp->init(program_path, backend)`。
   - 将 `visapp` 传给 `ViewportWidget`。
   - 移除 `RHIWindow.cpp` 中 handle==0 的临时 guard。

3. **PySide6 桥接方式对比**
   - 当前方案：C++ 返回 `WId`，Python 用 `shiboken6.wrapInstance` 包装。
   - 备选方案：扩展直接返回 `QWidget*` 的 `PyCapsule`，Python 用 `wrapInstance` 打开。
   - 若当前方案不稳定，可尝试第二种。

4. **GIL 与渲染线程**
   - 在 `create_viewport` / `update` 等可能耗时的地方加 `py::gil_scoped_release`。
   - 确保 LC 渲染回调不直接调用 Python。

5. **事件循环 ownership**
   - 确认 Python `QApplication.exec()` 运行后，C++ 不再创建第二个 `QApplication`。
   - 扩展内部已检查 `QCoreApplication::instance()` 并拒绝创建 QApp。

---

## 7. 风险提示

| 风险 | 当前状态 | 建议 |
|---|---|---|
| PySide6/Python 3.14 不兼容 | 已确认 | 降级到 Python 3.12/3.13。 |
| Qt ABI 不匹配 | 待用户环境确认 | 确保 PySide6 自带 Qt 与 rbc 编译 Qt 同版本。 |
| Stub 渲染器无画面 | 已知 | 替换为 VisApp/RenderAppBase。 |
| 跨语言 widget 父子关系 | 代码已写，待真机验证 | 若嵌入失败，尝试 winId/shiboken/capsule 多种方案。 |
| 退出崩溃 | 待真机验证 | 按 5.6 检查释放顺序。 |

---

## 8. 参考代码路径

- Python App：`src/robocute/app.py`
- 编辑器启动：`rbc/editor/editor/rbc_editor_module.cpp`
- 视口：`rbc/editor/runtime/src/ui/ViewportWidget.cpp`
- RHI 窗口：`rbc/editor/runtime/src/ui/RHIWindow.cpp`
- 渲染基类：`rbc/editor/runtime/include/RBCEditorRuntime/infra/render/app_base.h`
- Demo 扩展：`rbc/editor/python/`
- Demo Python：`samples/pyside_embed_demo/`

---

*本文档与代码一并作为 Demo 交接产物，后续调试与迭代由用户接管。*
