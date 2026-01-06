# Connection Plugin 测试环境

本文档描述了 `connection_plugin` 的测试环境和使用方法。

## 测试结构

测试环境按照文档 `docs/devlog/editor/EditorV03.md` 中的架构设计，采用 Mock 对象和分层测试策略：

```
rbc/editor/tests/
├── mocks/                          # Mock 对象
│   ├── MockConnectionService.h     # Mock ConnectionService
│   └── MockConnectionService.cpp
├── mvvm/                           # ViewModel 单元测试
│   └── test_connection_viewmodel.cpp
├── plugins/                        # Plugin 集成测试
│   └── test_connection_plugin.cpp
├── qml/                            # QML UI 测试
│   ├── test_connection_view.qml
│   └── test_connection_view_runner.cpp
└── xmake.lua                       # 测试构建配置
```

## Mock 对象

### MockConnectionService

`MockConnectionService` 提供了可控的 `ConnectionService` 行为，用于测试：

**主要功能：**
- 模拟连接成功/失败
- 模拟测试连接成功/失败
- 可配置的延迟连接
- 状态重置功能

**使用示例：**
```cpp
MockConnectionService *mockService = new MockConnectionService(this);

// 模拟成功连接
mockService->simulateConnectionSuccess();

// 模拟失败连接
mockService->simulateConnectionFailure("Network error");

// 设置延迟连接
mockService->setAutoConnectDelay(1000); // 1秒后自动连接
mockService->connect();
```

## 测试类型

### 1. ViewModel 单元测试 (`test_connection_viewmodel.cpp`)

测试 `ConnectionViewModel` 的行为，包括：

- **属性测试**：`serverUrl`, `connected`, `statusText`, `isBusy`
- **方法测试**：`setServerUrl`, `connect`, `disconnect`, `testConnection`
- **信号测试**：验证信号正确发射
- **集成测试**：完整的连接流程测试

**运行方式：**
```bash
xmake build test_editor_connection_viewmodel
xmake run test_editor_connection_viewmodel
```

### 2. Plugin 集成测试 (`test_connection_plugin.cpp`)

测试 `ConnectionPlugin` 的完整生命周期，包括：

- **生命周期测试**：`load`, `unload`, `reload`
- **元数据测试**：`id`, `name`, `version`, `dependencies`
- **View 贡献测试**：验证 View 贡献正确注册
- **ViewModel 创建测试**：验证 ViewModel 正确创建和绑定

**运行方式：**
```bash
xmake build test_editor_connection_plugin
xmake run test_editor_connection_plugin
```

### 3. QML UI 测试 (`test_connection_view.qml`)

测试 QML UI 组件的显示和交互，包括：

- View 初始化
- 状态指示器显示（连接/断开）
- 服务器 URL 输入
- 按钮启用/禁用状态
- 错误信息显示
- 加载指示器

**注意：** QML 测试文件通过 `test_resources.qrc` 资源文件包含在构建中，而不是直接添加到源文件列表。

**运行方式：**
```bash
xmake build test_editor_connection_view_qml
xmake run test_editor_connection_view_qml
```

## 测试场景

### 场景 1：正常连接流程

```cpp
// 1. 设置服务器 URL
viewModel->setServerUrl("http://127.0.0.1:5555");

// 2. 测试连接
viewModel->testConnection();
mockService->simulateTestSuccess();

// 3. 连接
viewModel->connect();
mockService->simulateConnectionSuccess();

// 4. 验证状态
QCOMPARE(viewModel->connected(), true);
QCOMPARE(viewModel->statusText(), QString("Connected"));
```

### 场景 2：连接失败处理

```cpp
viewModel->setServerUrl("http://invalid:5555");
viewModel->connect();
mockService->simulateConnectionFailure("Connection timeout");

QCOMPARE(viewModel->connected(), false);
QCOMPARE(viewModel->statusText(), QString("Connection timeout"));
```

### 场景 3：Plugin 热重载

```cpp
// 加载 Plugin
plugin->load(context);
mockService->setServerUrl("http://test:5555");

// 重载 Plugin
plugin->reload();

// 验证状态保持
QCOMPARE(mockService->serverUrl(), QString("http://test:5555"));
```

## Mock 环境配置

### 不同测试环境的 Mock 配置

#### 1. 成功连接环境
```cpp
MockConnectionService *mockService = new MockConnectionService(this);
mockService->setAutoConnectDelay(0); // 立即连接
// 默认行为：连接成功
```

#### 2. 失败连接环境
```cpp
MockConnectionService *mockService = new MockConnectionService(this);
// 重写 connect 方法或使用 simulateConnectionFailure
```

#### 3. 延迟连接环境
```cpp
MockConnectionService *mockService = new MockConnectionService(this);
mockService->setAutoConnectDelay(1000); // 1秒延迟
```

#### 4. 网络错误环境
```cpp
MockConnectionService *mockService = new MockConnectionService(this);
mockService->simulateConnectionFailure("Network unreachable");
```

## 运行所有测试

```bash
# 构建所有测试
xmake build test_editor_connection_viewmodel
xmake build test_editor_connection_plugin
xmake build test_editor_connection_view_qml

# 运行所有测试
xmake run test_editor_connection_viewmodel
xmake run test_editor_connection_plugin
xmake run test_editor_connection_view_qml
```

## 测试覆盖率

当前测试覆盖：

- ✅ ViewModel 属性访问和修改
- ✅ ViewModel 方法调用
- ✅ ViewModel 信号发射
- ✅ Plugin 生命周期管理
- ✅ Plugin View 贡献
- ✅ ViewModel 创建和绑定
- ✅ QML UI 组件显示
- ✅ 错误处理

## 注意事项

1. **单例模式**：`EditorPluginManager` 是单例，测试后需要清理状态
2. **信号传播**：某些测试需要调用 `QCoreApplication::processEvents()` 来确保信号传播
3. **QML 测试**：QML 测试需要 Qt Quick Test 框架支持
4. **Mock 对象生命周期**：确保 Mock 对象在测试期间保持有效

## 扩展测试

要添加新的测试场景：

1. 在相应的测试文件中添加新的测试方法
2. 使用 `MockConnectionService` 模拟不同的环境
3. 验证 ViewModel 和 UI 的正确响应

## 参考文档

- [EditorV03.md](../../../docs/devlog/editor/EditorV03.md) - 架构设计文档
- [ConnectionPlugin.h](../../plugins/connection_plugin/ConnectionPlugin.h) - Plugin 实现
- [ConnectionService.h](../../runtimex/include/RBCEditorRuntime/services/ConnectionService.h) - Service 接口

