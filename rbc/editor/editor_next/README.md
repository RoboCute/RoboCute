# RoboCute Editor (QML Version)

这是 RoboCute Editor 的 QML 版本，支持 Qt Creator 热重载和组件调试。

## 快速开始

### 构建

#### 使用 xmake（命令行）

```bash
# 构建 QML 版本的编辑器
xmake build rbc_editor_next

# 运行
xmake run rbc_editor_next
```

#### 使用 CMake（Qt Creator 推荐）

```bash
# 配置 CMake
cmake -B build_cmake -S . -DQt6_DIR=/path/to/Qt/6.x.x/lib/cmake/Qt6

# 构建
cmake --build build_cmake --target rbc_editor_next

# 运行
./build_cmake/bin/rbc_editor_next  # Linux/Mac
# 或
build_cmake\bin\Debug\rbc_editor_next.exe  # Windows
```

## Qt Creator 完整使用指南

### 1. 打开项目

1. **启动 Qt Creator**
2. **打开项目**：
   - 菜单：`文件` -> `打开文件或项目...`
   - 选择项目根目录的 `CMakeLists.txt`
   - 点击 `打开`
3. **配置 CMake**：
   - Qt Creator 会自动检测 CMake
   - 如果 Qt6 未自动找到，在配置页面设置：
     - `Qt6_DIR`: `/path/to/Qt/6.x.x/lib/cmake/Qt6`
     - 或 `Qt6_ROOT`: `/path/to/Qt/6.x.x`
   - 点击 `配置项目`

### 2. 配置运行和调试

#### 2.1 选择运行目标

1. 在左侧项目树中，展开 `rbc_editor_next`
2. 右键点击 `rbc_editor_next` -> `设置为活动项目`
3. 在运行配置下拉菜单中选择 `rbc_editor_next`

#### 2.2 配置运行设置

1. 点击左侧 `项目` 图标（或按 `Alt+7`）
2. 选择 `运行` 标签页
3. 在 `运行配置` 中：
   - **可执行文件**: 应该自动设置为 `rbc_editor_next`
   - **工作目录**: 设置为项目根目录（`${PROJECT_DIR}`）
   - **运行环境**: 点击 `详情` 添加以下环境变量：
     ```
     QML_DISABLE_DISK_CACHE=1
     ```
     这确保 QML 文件不会被缓存，修改后立即生效

#### 2.3 启用 QML 调试

**重要**: QML 调试必须在运行前配置！

1. 在 `项目` -> `运行` 标签页中
2. 找到 `QML Debugging` 部分
3. 勾选 **`Enable QML Debugging`**
4. 设置调试端口（默认 `3768`，通常不需要修改）
5. 选择调试类型：
   - **`QML Profiler`**: 性能分析
   - **`QML Debugger`**: 代码调试（断点、变量查看等）
   - **`Both`**: 同时启用（推荐）

#### 2.4 配置调试设置

1. 切换到 `调试` 标签页
2. 确保 `调试器类型` 设置为 `GDB`（Linux/Mac）或 `CDB`（Windows）
3. 在 `启动和结束` 部分：
   - 可以设置启动时自动附加到 QML 调试器

### 3. QML 热重载

#### 3.1 启用热重载

热重载需要满足以下条件：

1. **使用文件系统 QML**（开发模式）：
   - 程序会自动检测文件系统中的 QML 文件
   - 如果存在 `rbc/editor/editor_next/qml/MainWindow.qml`，会自动使用文件系统版本
   - 如果不存在，会使用资源文件（不支持热重载）

2. **启用 QML 调试**（见上一节）

3. **运行程序**：
   - 点击 `运行` 按钮（绿色三角形）或按 `Ctrl+R`
   - 程序启动后，QML 调试器会自动连接

#### 3.2 使用热重载

**方法 1：自动重载（推荐）**
- 修改 QML 文件并保存
- Qt Creator 会自动检测更改并提示重载
- 点击提示中的 `重新加载 QML` 按钮

**方法 2：手动重载**
- 修改 QML 文件后
- 按 `Ctrl+R`（或 `F5`）
- 或使用菜单：`调试` -> `重新加载 QML`

**方法 3：使用 QML 调试器面板**
- 在 `窗口` -> `视图` -> `QML/JS 调试控制台` 中
- 点击 `重新加载 QML` 按钮

#### 3.3 热重载限制

- ✅ **支持**: QML 属性值、布局、样式、绑定表达式
- ❌ **不支持**: 
  - 添加/删除组件（需要重启程序）
  - 修改 C++ 代码（需要重新编译）
  - 修改 `qmldir` 文件（需要重启程序）
  - 修改资源文件（需要重新编译）

### 4. QML 调试

#### 4.1 设置断点

1. **在 QML 文件中设置断点**：
   - 打开 QML 文件（如 `MainWindow.qml`）
   - 在行号左侧点击，设置断点（红色圆点）
   - 断点可以设置在：
     - JavaScript 表达式
     - 属性绑定
     - 信号处理器
     - 函数调用

2. **在 JavaScript 中设置断点**：
   - 在 `onClicked`、`onCompleted` 等处理器中
   - 在 `function` 定义中

#### 4.2 调试操作

**启动调试**：
- 点击 `调试` 按钮（带虫子的绿色三角形）或按 `F5`
- 程序会在断点处暂停

**调试控制**：
- **继续执行** (`F5`): 继续运行到下一个断点
- **单步跳过** (`F10`): 执行当前行，不进入函数
- **单步进入** (`F11`): 进入函数内部
- **单步跳出** (`Shift+F11`): 跳出当前函数
- **停止调试** (`Shift+F5`): 停止程序

#### 4.3 查看变量和表达式

1. **局部变量窗口**：
   - `窗口` -> `视图` -> `局部变量和表达式`
   - 显示当前作用域的所有变量

2. **监视表达式**：
   - `窗口` -> `视图` -> `监视`
   - 添加要监视的表达式（如 `connectionService.connected`）

3. **QML 对象检查器**：
   - `窗口` -> `视图` -> `QML/JS 调试控制台`
   - 可以查看 QML 对象树和属性

4. **控制台输出**：
   - `窗口` -> `视图` -> `应用程序输出`
   - 查看 `console.log()` 输出

#### 4.4 调试技巧

**查看 QML 对象属性**：
```qml
// 在 QML 中添加调试输出
Component.onCompleted: {
    console.log("Window width:", width)
    console.log("ConnectionService:", connectionService)
    console.log("Connected:", connectionService.connected)
}
```

**使用调试器表达式求值**：
- 在断点暂停时，可以在 `局部变量和表达式` 窗口中输入表达式
- 例如：`connectionService ? connectionService.connected : false`

**查看调用堆栈**：
- `窗口` -> `视图` -> `调用堆栈`
- 显示从程序入口到当前断点的调用链

### 5. QML Profiler（性能分析）

#### 5.1 启用 Profiler

1. 在运行配置中启用 `QML Profiler`
2. 运行程序并执行要分析的操作
3. 停止程序后，Profiler 会自动打开

#### 5.2 分析性能

Profiler 显示：
- **时间线**: 显示各个操作的执行时间
- **事件统计**: 显示事件类型和次数
- **内存使用**: 显示内存分配情况

**常见性能问题**：
- 频繁的属性绑定更新
- 大量的 JavaScript 计算
- 复杂的布局计算

### 6. 常见问题排查

#### 问题 1: 热重载不工作

**症状**: 修改 QML 文件后，界面没有更新

**解决方案**:
1. 检查是否使用了文件系统 QML：
   - 查看控制台输出，应该显示 `Using file system QML (hot reload enabled)`
   - 如果显示 `Using resource QML`，需要确保文件系统路径存在
2. 检查 QML 调试是否启用
3. 检查环境变量 `QML_DISABLE_DISK_CACHE=1` 是否设置
4. 尝试手动重载：`Ctrl+R`

#### 问题 2: QML 调试器无法连接

**症状**: 启动程序后，调试器显示连接失败

**解决方案**:
1. 检查防火墙是否阻止了端口 `3768`
2. 检查是否有多个 Qt Creator 实例运行
3. 尝试更改调试端口（在运行配置中）
4. 确保程序是以调试模式运行的（不是 Release）

#### 问题 3: 断点不生效

**症状**: 设置了断点，但程序不暂停

**解决方案**:
1. 确保 QML 调试已启用
2. 确保断点设置在有效的代码行（不是注释或空行）
3. 检查断点是否在正确的文件中（文件系统 vs 资源文件）
4. 尝试清理并重新构建项目

#### 问题 4: 修改 C++ 代码后 QML 不更新

**症状**: 修改了 C++ 代码，但 QML 中调用该代码的地方没有变化

**解决方案**:
- C++ 代码修改后必须重新编译
- QML 热重载只适用于 QML 文件
- 重新构建项目：`构建` -> `重新构建项目` 或 `Ctrl+Shift+B`

#### 问题 5: 找不到 QML 文件

**症状**: 程序启动时显示 `QML file not found`

**解决方案**:
1. 检查资源文件 `rbc_editor.qrc` 是否包含 QML 文件
2. 检查文件路径是否正确
3. 确保 CMake 配置正确，资源文件被正确编译

### 7. 开发工作流建议

#### 推荐的开发流程：

1. **首次设置**：
   ```
   1. 在 Qt Creator 中打开项目
   2. 配置运行和调试设置
   3. 启用 QML 调试
   4. 运行一次程序，确保一切正常
   ```

2. **日常开发**：
   ```
   1. 启动程序（调试模式）
   2. 修改 QML 文件
   3. 保存文件，自动或手动重载
   4. 查看效果，继续迭代
   ```

3. **调试问题**：
   ```
   1. 设置断点
   2. 重现问题
   3. 检查变量和调用堆栈
   4. 修复问题，重载验证
   ```

4. **性能优化**：
   ```
   1. 启用 QML Profiler
   2. 运行程序，执行操作
   3. 分析性能数据
   4. 优化瓶颈
   ```

### 8. 快捷键参考

| 操作 | 快捷键 |
|------|--------|
| 运行程序 | `Ctrl+R` |
| 调试程序 | `F5` |
| 停止调试 | `Shift+F5` |
| 重新加载 QML | `Ctrl+R` (运行时) |
| 单步跳过 | `F10` |
| 单步进入 | `F11` |
| 单步跳出 | `Shift+F11` |
| 继续执行 | `F5` |
| 切换断点 | `F9` |
| 构建项目 | `Ctrl+B` |
| 重新构建 | `Ctrl+Shift+B` |

## 架构说明

### 目录结构

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

## 9. 实用提示

### 9.1 快速检查清单

在开始开发前，确保：

- [ ] Qt Creator 已正确配置 Qt 6.x
- [ ] CMake 配置成功，没有错误
- [ ] 运行配置中已启用 QML 调试
- [ ] 环境变量 `QML_DISABLE_DISK_CACHE=1` 已设置
- [ ] 工作目录设置为项目根目录
- [ ] 程序可以正常启动

### 9.2 开发模式 vs 生产模式

**开发模式**（推荐用于日常开发）：
- 使用文件系统 QML（支持热重载）
- 启用 QML 调试
- 启用详细日志
- 工作目录：项目根目录

**生产模式**（用于最终发布）：
- 使用资源文件 QML（性能更好）
- 禁用 QML 调试
- 禁用详细日志
- 优化编译选项

### 9.3 性能优化建议

1. **减少不必要的绑定**：
   ```qml
   // 不好：每次都会重新计算
   Text { text: someObject.property1 + someObject.property2 }
   
   // 好：使用中间属性
   property string combinedText: someObject.property1 + someObject.property2
   Text { text: combinedText }
   ```

2. **避免在绑定中进行复杂计算**：
   ```qml
   // 不好：复杂计算在绑定中
   Rectangle { 
       width: Math.max(100, Math.min(500, parent.width * 0.8)) 
   }
   
   // 好：使用函数
   function calculateWidth() {
       return Math.max(100, Math.min(500, parent.width * 0.8))
   }
   Rectangle { width: calculateWidth() }
   ```

3. **使用 Loader 延迟加载**：
   ```qml
   // 延迟加载复杂组件
   Loader {
       active: shouldLoadComponent
       source: "ComplexComponent.qml"
   }
   ```

### 9.4 调试技巧

**使用 console.log()**：
```qml
Component.onCompleted: {
    console.log("Component loaded:", objectName)
    console.log("Properties:", JSON.stringify({
        width: width,
        height: height
    }))
}
```

**使用调试器表达式**：
- 在断点处，可以在 `局部变量和表达式` 窗口中输入任何有效的 QML/JavaScript 表达式
- 例如：`connectionService ? connectionService.serverUrl : "N/A"`

**查看对象树**：
- 使用 QML 对象检查器查看完整的对象树
- 可以查看每个对象的属性值

## 10. 故障排除

### 常见错误信息

**错误**: `ConnectionStatusPanel is not a type`
- **原因**: QML 模块未正确注册或导入路径错误
- **解决**: 检查 `qmldir` 文件是否正确，确保资源文件包含所有 QML 文件

**错误**: `QML Debugging: Connection refused`
- **原因**: 调试服务未启用或端口被占用
- **解决**: 确保在 Debug 模式下编译，检查端口 3768 是否可用

**错误**: `Cannot find module "RBCEditor"`
- **原因**: QML 类型未正确注册
- **解决**: 检查 `registerQmlTypes()` 是否被调用，确保在创建引擎之前注册

## 11. 参考资源

- [Qt Creator QML 调试文档](https://doc.qt.io/qtcreator/creator-debugging-qml.html)
- [Qt Quick QML 文档](https://doc.qt.io/qt-6/qtqml-index.html)
- [Qt Creator 用户手册](https://doc.qt.io/qtcreator/index.html)
- [QML 最佳实践](https://doc.qt.io/qt-6/qtquick-bestpractices.html)

## 12. 更新日志

### 2026-01-01
- ✅ 添加 CMake 支持
- ✅ 添加 QML 调试支持
- ✅ 添加日志 dump 功能
- ✅ 完善 README 文档
- ✅ 修复 ConnectionStatusPanel 导入问题
