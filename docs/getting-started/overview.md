# Overview

RoboCute is a Python-first 3D AIGC/Robotics development tool with a node-based workflow.

## Architecture

RoboCute follows a server-client architecture:

- **Python Server**: Core logic, scene management, node execution
- **C++ Editor**: Optional visualization and debugging tool

## Core Concepts

### Scene

The scene is the central data structure that manages all entities, components, and resources.

### Nodes

Nodes are reusable algorithm units that can be connected together to form complex workflows.

### Editor

The editor provides a visual interface for:
- Scene visualization
- Node graph editing
- Animation playback
- Parameter adjustment

## Workflow

1. Create a scene in Python
2. Define nodes and connect them
3. Execute the node graph
4. Visualize results in the editor (optional)

