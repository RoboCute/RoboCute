# QML Editor Architecture

这是 RoboCute Editor 的 QML 版本架构，支持 Qt Creator 热重载和组件调试。

## 架构概述

### 目录结构

```
rbc/editor/
├── runtime_next/          # QML 版本的运行时库
│   ├── include/
│   │   └── RBCEditorRuntime/
│   │       └── qml/       # QML 后端服务（C++）
│   │           ├── ConnectionService.h
│   │           ├── EditorService.h
│   │           └── QmlTypes.h
│   ├── src/
│   │   └── qml/           # QML 后端服务实现
│   │       ├── ConnectionService.cpp
│   │       ├── EditorService.cpp
│   │       └── QmlTypes.cpp
│   └── qml/               # QML UI 文件
│       ├── MainWindow.qml
│       └── components/
│           └── ConnectionStatusPanel.qml
└── editor_next/           # QML 版本的主程序
    └── main.cpp
```

## 核心组件

### 1. C++ 后端服务

#### ConnectionService
- **功能**：管理后端服务器连接状态
- **QML 属性**：
  - `serverUrl`: 服务器 URL
  - `connected`: 连接状态
  - `statusText`: 状态文本
- **方法**：
  - `testConnection()`: 测试连接
  - `connect()`: 开始连接
  - `disconnect()`: 断开连接

#### EditorService
- **功能**：编辑器主服务
- **QML 属性**：
  - `version`: 版本号
- **方法**：
  - `initialize()`: 初始化编辑器
  - `shutdown()`: 关闭编辑器

### 2. QML UI 组件

#### MainWindow.qml
主窗口，包含：
- 左侧面板区域
- 中央内容区域
- 服务初始化和管理

#### ConnectionStatusPanel.qml
连接状态面板（迁移示例）：
- 从 Widget 版本 (`ConnectionStatusView`) 迁移而来
- 展示连接状态
- 提供连接测试和连接/断开功能

## 使用方法

### 构建

```bash
xmake build rbc_editor_next
```

### 运行

```bash
xmake run rbc_editor_next
```

## Qt Creator 热重载

### 配置步骤

1. **打开项目**
   - 在 Qt Creator 中打开项目
   - 确保项目使用 CMake 或 xmake（需要 Qt Creator 插件支持）

2. **配置 QML 调试**
   - 打开 `工具` -> `选项` -> `Qt Quick`
   - 启用 "Enable QML Debugging"
   - 设置调试端口（默认 3768）

3. **运行配置**
   - 在运行配置中添加环境变量：
     ```
     QML_DISABLE_DISK_CACHE=1
     ```
   - 这会禁用 QML 磁盘缓存，确保修改立即生效

4. **热重载使用**
   - 运行程序后，修改 QML 文件
   - 按 `Ctrl+R`（或 `F5`）重新加载 QML
   - 或者使用菜单：`调试` -> `重新加载 QML`

### 开发模式 vs 生产模式

#### 开发模式（文件系统路径）
程序会自动检测文件系统中的 QML 文件：
```cpp
QString fsPath = QDir::currentPath() + "/rbc/editor/runtime_next/qml/MainWindow.qml";
if (QFile::exists(fsPath)) {
    actualQmlFile = QUrl::fromLocalFile(fsPath);
}
```

优点：
- ✅ 支持热重载
- ✅ 修改后立即生效
- ✅ 方便调试

#### 生产模式（资源文件）
使用编译到资源文件中的 QML：
```cpp
const QUrl qmlFile(QStringLiteral("qrc:/qml/MainWindow.qml"));
```

优点：
- ✅ 单文件部署
- ✅ 保护源代码
- ✅ 加载更快

## 迁移示例：ConnectionStatusPanel

### Widget 版本（原始）
```cpp
// ConnectionStatusView.cpp
class ConnectionStatusView : public QWidget {
    // 使用 QVBoxLayout、QLabel、QLineEdit、QPushButton 等
    // 约 200+ 行代码
};
```

### QML 版本（迁移后）
```qml
// ConnectionStatusPanel.qml
Rectangle {
    // 声明式 UI，约 150 行代码
    // 支持动画、状态绑定等
}
```

### 优势对比

| 特性 | Widget 版本 | QML 版本 |
|------|------------|---------|
| 代码行数 | ~200 行 | ~150 行 |
| 热重载 | ❌ 需要重新编译 | ✅ Ctrl+R 即可 |
| 动画支持 | 需要 QPropertyAnimation | ✅ 内置动画 |
| 状态绑定 | 手动 connect | ✅ 自动绑定 |
| Qt Creator 预览 | ❌ | ✅ 可视化设计器 |
| 调试支持 | 有限 | ✅ QML Profiler/Debugger |

## 扩展指南

### 添加新的 QML 服务

1. **创建 C++ 服务类**
```cpp
// MyService.h
class MyService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString data READ data NOTIFY dataChanged)
public:
    // ...
};
```

2. **注册到 QML**
```cpp
// QmlTypes.cpp
qmlRegisterType<MyService>("RBCEditor", 1, 0, "MyService");
```

3. **在 QML 中使用**
```qml
import RBCEditor 1.0

MyService {
    id: myService
    onDataChanged: console.log("Data changed:", data)
}
```

### 迁移 Widget 组件到 QML

1. **分析 Widget 组件**
   - 识别 UI 元素（按钮、输入框、标签等）
   - 识别状态和属性
   - 识别信号和槽

2. **创建 QML 组件**
   - 使用对应的 QML 元素
   - 使用属性绑定替代信号槽
   - 添加动画和过渡效果

3. **连接 C++ 后端**
   - 如果组件需要 C++ 逻辑，创建对应的服务类
   - 通过 Context Property 或注册类型暴露给 QML

## 调试技巧

### QML Debugger
- 设置断点：在 QML 代码中点击行号
- 检查属性：在调试器中查看对象属性
- 控制台输出：使用 `console.log()`

### QML Profiler
- 性能分析：查看渲染时间、JavaScript 执行时间
- 内存分析：检查对象创建和销毁

### 常见问题

1. **QML 文件找不到**
   - 检查资源文件（.qrc）配置
   - 检查文件路径是否正确

2. **热重载不工作**
   - 确保使用文件系统路径（开发模式）
   - 检查 `QML_DISABLE_DISK_CACHE` 环境变量
   - 重启 Qt Creator

3. **C++ 类型未注册**
   - 检查 `registerQmlTypes()` 是否调用
   - 检查命名空间和版本号

## 下一步计划

- [ ] 迁移更多组件（DetailsPanel、SceneHierarchy 等）
- [ ] 添加视口组件（需要特殊处理 RHI 集成）
- [ ] 实现节点编辑器（可能需要保留 Widget 或寻找 QML 替代方案）
- [ ] 完善主题和样式系统
- [ ] 添加更多动画和过渡效果

## 参考资源

- [Qt QML Documentation](https://doc.qt.io/qt-6/qtqml-index.html)
- [Qt Quick Controls 2](https://doc.qt.io/qt-6/qtquickcontrols2-index.html)
- [QML Hot Reload Guide](https://doc.qt.io/qtcreator/creator-qml-modules.html)

