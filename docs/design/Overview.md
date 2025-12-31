# Robocute设计总览

Robocute旨在为3D-AIGC和机器人相关的任务提供一个简单易用的工具集，包括

- 简单直接的python-API
- 灵活高效的编辑器
- 方便的扩展设计

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
