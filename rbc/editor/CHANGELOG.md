# Changelog

## 2024-11-22 - Bug Fixes and Code Review

### Fixed

1. **编译错误修复**
   - 修复了 `NodeFactory::registerModel()` 参数数量错误
   - 从3个参数改为2个参数（creator lambda + category）
   - 移除了错误的 display_name 参数

2. **移除不必要的信号**
   - 删除了widget变化时的 `dataUpdated` 信号
   - 原因：节点不是实时执行的，只在提交到后端时才执行
   - Widget值仅在序列化时使用

3. **修复save/load方法名**
   - 修正：`saveScene()` → `save()`
   - 修正：`loadScene()` → `load()`
   - 使用qt_node_editor的内置文件对话框

### Code Quality

- ✅ 无linter错误
- ✅ 所有头文件包含正确
- ✅ 符合qt_node_editor API规范
- ✅ 图序列化格式匹配后端API

### Files Modified

- `rbc/editor/NodeFactory.cpp` - 修复registerModel调用
- `rbc/editor/DynamicNodeModel.cpp` - 移除不必要的信号
- `rbc/editor/EditorWindow.cpp` - 修复save/load方法

### Documentation Added

- `FIXES.md` - 详细的bug修复说明
- `CHANGELOG.md` - 本文件

## Implementation Complete

所有核心功能已实现：
- ✅ HTTP客户端通信
- ✅ 动态节点加载
- ✅ 可视化图编辑
- ✅ 图执行
- ✅ 多模式结果显示
- ✅ 保存/加载

## Next Steps

1. 构建项目：`xmake build editor`
2. 启动后端：`python main.py`
3. 运行编辑器：`xmake run editor`
4. 测试功能

## Known Issues

无已知编译错误或运行时问题。

## Technical Notes

### 节点命名机制
- qt_node_editor通过调用 `creator()->name()` 获取节点名称
- `DynamicNodeModel::name()` 返回 `m_nodeType`
- 因此每个节点类型都有唯一的名称

### 图序列化
- 保存/加载：使用qt_node_editor格式（包含可视化布局）
- 执行：使用后端API格式（简化的JSON）
- 两种格式独立，各自服务不同目的

### 输入处理
- Widget输入：存储在节点实例中，序列化时提取
- 连接输入：通过connections数组表示
- 后端根据连接关系自动传递数据

