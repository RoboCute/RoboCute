# RoboCute Editor (QML Version)

这是 RoboCute Editor 的 QML 版本，支持 Qt Creator 热重载和组件调试。

## 快速开始

### 构建

```bash
# 构建 QML 版本的编辑器
xmake build rbc_editor_next

# 运行
xmake run rbc_editor_next
```

### 在 Qt Creator 中使用

1. **打开项目**
   - 在 Qt Creator 中打开项目根目录
   - 确保已配置 Qt 6.x 工具链

2. **配置运行**
   - 选择 `rbc_editor_next` 作为运行目标
   - 在运行配置中添加环境变量：
     ```
     QML_DISABLE_DISK_CACHE=1
     ```

3. **启用 QML 调试**
   - `工具` -> `选项` -> `Qt Quick`
   - 启用 "Enable QML Debugging"
   - 设置调试端口（默认 3768）

4. **热重载**
   - 运行程序后，修改 QML 文件
   - 按 `Ctrl+R` 或 `F5` 重新加载 QML
   - 或使用菜单：`调试` -> `重新加载 QML`

## 架构说明

### 目录结构

```
rbc/editor/
├── runtime_next/          # QML 运行时库
│   ├── include/RBCEditorRuntime/qml/
│   │   ├── ConnectionService.h    # 连接服务
│   │   ├── EditorService.h        # 编辑器服务
│   │   └── QmlTypes.h             # QML 类型注册
│   ├── src/qml/                   # 服务实现
│   └── qml/                       # QML UI 文件
│       ├── MainWindow.qml
│       └── components/
│           └── ConnectionStatusPanel.qml
└── editor_next/           # 主程序
    └── main.cpp
```

### 核心组件

#### C++ 后端服务

- **ConnectionService**: 管理后端服务器连接
- **EditorService**: 编辑器主服务

#### QML UI 组件

- **MainWindow.qml**: 主窗口
- **ConnectionStatusPanel.qml**: 连接状态面板（迁移示例）

## 迁移示例

`ConnectionStatusPanel.qml` 是从 Widget 版本的 `ConnectionStatusView` 迁移而来的示例：

- **原始版本**: `rbc/editor/runtime/src/components/ConnectionStatusView.cpp`
- **QML 版本**: `rbc/editor/runtime_next/qml/components/ConnectionStatusPanel.qml`

### 优势

- ✅ 代码更简洁（~150 行 vs ~200 行）
- ✅ 支持热重载（无需重新编译）
- ✅ 内置动画支持
- ✅ 自动状态绑定
- ✅ Qt Creator 可视化预览

## 开发模式 vs 生产模式

程序会自动检测：
- **开发模式**: 如果存在文件系统路径，使用文件系统 QML（支持热重载）
- **生产模式**: 使用编译到资源文件中的 QML

## 下一步

- [ ] 迁移更多组件（DetailsPanel、SceneHierarchy 等）
- [ ] 添加视口组件（需要特殊处理 RHI 集成）
- [ ] 实现节点编辑器
- [ ] 完善主题系统

## 参考

详细文档请参考：`rbc/editor/runtime_next/README_QML.md`

