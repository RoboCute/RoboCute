# RoboCute Development Log / 开发日志

This directory contains development logs and milestone records for the RoboCute project.

本目录包含 RoboCute 项目的开发日志和里程碑记录。

## 📋 Table of Contents / 目录

### Version Milestones / 版本里程碑

- [v0.1 MVP](version/v01.md) - Minimum Viable Product (Completed / 已完成)
- [v0.2](version/v02.md) - First Refactoring (In Progress / 进行中)

### Editor Development / 编辑器开发

- [Editor V0.1](editor/EditorV01.md) - Initial Editor Implementation (Completed / 已完成)
- [Editor V0.2](editor/EditorV02.md) - MainWindow Refactoring (In Progress / 进行中)

## 📊 Development Timeline / 开发时间线

### v0.1 MVP (Completed / 已完成)

**English:**

The first milestone focused on creating a minimal viable product that demonstrates the core concept:

- ✅ Python backend with node system
- ✅ C++ editor with basic UI
- ✅ Scene management and entity system
- ✅ Animation workflow (create → execute → playback)
- ✅ Server-client communication via REST API

**中文:**

第一个里程碑专注于创建一个展示核心概念的最小可行产品：

- ✅ 带节点系统的 Python 后端
- ✅ 带基础 UI 的 C++ 编辑器
- ✅ 场景管理和实体系统
- ✅ 动画工作流（创建 → 执行 → 播放）
- ✅ 通过 REST API 进行服务器-客户端通信

### v0.2 (In Progress / 进行中)

**English:**

The second milestone focuses on refactoring and improving the codebase:

- 🚧 Python codegen workflow refactoring
- 🚧 C++ world_v2 scene and resource management system
- 🚧 Improved editor GUI with better stability
- 🚧 Basic raster renderer (grid lines, selection outlines, camera)
- 🚧 AIGC nodes integration (text2image, text2model, text2anim)
- 🚧 Physics nodes integration (UIPC rigid body physics)

**中文:**

第二个里程碑专注于重构和改进代码库：

- 🚧 Python 代码生成工作流重构
- 🚧 C++ world_v2 场景和资源管理系统
- 🚧 改进的编辑器 GUI，提高稳定性
- 🚧 基础光栅渲染器（网格线、选择描边、相机）
- 🚧 AIGC 节点集成（text2image、text2model、text2anim）
- 🚧 物理节点集成（UIPC 刚体物理）

## 🎯 Key Design Principles / 核心设计原则

**English:**

1. **Python-First**: Python is the single source of truth for all scene data and logic
2. **Optional Editor**: The editor is a debugging/visualization tool, not required for core functionality
3. **Command Pattern**: Editor sends commands to server, server broadcasts updates back
4. **Headless Support**: Full support for offline rendering and simulations without GUI
5. **Extensible**: Easy to create custom nodes and extend functionality

**中文:**

1. **Python-First**: Python 是所有场景数据和逻辑的唯一真实来源
2. **可选编辑器**: 编辑器是调试/可视化工具，不是核心功能的必需组件
3. **命令模式**: 编辑器向服务器发送命令，服务器广播更新
4. **无头支持**: 完全支持无 GUI 的离线渲染和仿真
5. **可扩展**: 易于创建自定义节点和扩展功能

## 📝 How to Contribute / 如何贡献

**English:**

When documenting new features or milestones:

1. Create a new markdown file in the appropriate subdirectory
2. Follow the existing format and structure
3. Include both English and Chinese descriptions when possible
4. Update this README with links to new documents
5. Add completion dates and status indicators

**中文:**

在记录新功能或里程碑时：

1. 在相应的子目录中创建新的 markdown 文件
2. 遵循现有的格式和结构
3. 尽可能包含英文和中文描述
4. 更新此 README，添加新文档的链接
5. 添加完成日期和状态指示器

