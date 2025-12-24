# Welcome to RoboCute

<div align="center">

**Make Robotics Cute!**

A Python-first 3D AIGC/Robotics development tool with node-based workflow

[![Python](https://img.shields.io/badge/Python-3.12+-blue.svg)](https://www.python.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](../Licenses/Apache-2.0.txt)
[![Status](https://img.shields.io/badge/Status-Early%20Development-orange.svg)](devlog/README.md)

</div>

---

## What is RoboCute? / 什么是 RoboCute?

**English:**

RoboCute is a Python-first 3D AIGC (AI-Generated Content) and Robotics development tool that adopts a ComfyUI-like node-based server-client architecture. It comes with a self-developed cross-platform graphics engine, runtime, and editor, allowing you to write node algorithms directly in Python, seamlessly integrating with the entire Python ecosystem.

**中文:**

RoboCute 是一个 Python-first 的 3D AIGC（AI生成内容）和机器人开发工具，采用类似 ComfyUI 的节点式 server-client 架构。它配备了自研的跨平台图形引擎、运行时和编辑器，允许您直接用 Python 编写节点算法，无缝接入整个 Python 生态。

## Key Features / 核心特性

- 🐍 **Python-First**: All core logic runs in Python
- 🎨 **Node-Based**: Visual node graph editing
- 🎬 **Scene Management**: Complete ECS-based scene system
- 🎭 **Animation**: Rich animation system with timeline
- 🔬 **Physics**: Rigid body physics simulation
- 🤖 **AIGC**: Built-in text2image, text2model, text2anim nodes
- 🖥️ **Cross-Platform**: Native desktop editor

## Quick Start / 快速开始

```python
import robocute as rbc

# Create a scene
scene = rbc.Scene()
scene.start()

# Create an entity
robot = scene.create_entity("Robot")
scene.add_component(
    robot.id,
    "transform",
    rbc.TransformComponent(position=[0, 0, 0])
)

# Start server
server = rbc.Server(title="RoboCute Server")
editor_service = rbc.EditorService(scene)
server.register_service(editor_service)
server.start(port=5555)
```

## Documentation / 文档

- 📖 [Getting Started](getting-started/overview.md)
- 🛠️ [Build Guide](../BUILD.md)
- 📝 [Development Log](devlog/README.md)
- 🎨 [Architecture](design/Architecture.md)

## Project Status / 项目状态

RoboCute is currently in **early development**. See [Development Log](devlog/README.md) for progress.

RoboCute 目前处于**早期开发阶段**。查看[开发日志](devlog/README.md)了解进度。

