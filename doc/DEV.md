# Development Plan

This document outlines the development roadmap for RoboCute, focusing on the implementation of the Python-First, ComfyUI-like workflow defined in the PRD.

## Phase 1: Foundation & Code Generation (Current Focus)
**Goal**: Establish the bridge between Python definitions and C++ implementation.

1.  **Type System & Reflection**:
    - [x] Basic Type registration (Vector, Matrix).
    - [ ] Implement `Serde` (Serialization/Deserialization) for core types.
    - [ ] Python -> C++ codegen pipeline for shared data structures.
2.  **Node Interface Definition**:
    - Define the Python base class `RBCNode`.
    - Create schemas for `InputPort`, `OutputPort`, and `NodeMetadata`.
    - Implement codegen to export these definitions to C++ headers.

## Phase 2: Core Framework Implementation
**Goal**: Build the standalone Editor and the Python Runtime Engine.

1.  **Python Runtime Engine**:
    - Implement `NodeRegistry` to collect all available nodes at startup.
    - Implement `GraphExecutor`: A topological sorter and executor for the node graph.
2.  **C++ Editor Framework**:
    - Setup `RBCEditorStandalone` application (Qt setup).
    - Create a basic `NodeGraphWidget` (using Qt Nodes or custom QGraphicsView).
    - Implement dynamic node palette population based on JSON/Schema from Python.

## Phase 3: IPC & Workflow Integration
**Goal**: Connect the Python Host and C++ Editor.

1.  **Communication Layer (IPC)**:
    - Choose a transport protocol (ZeroMQ, TCP, or Shared Memory).
    - **Python Server**: Listens for "GetNodes", "UpdateScene", "RunGraph".
    - **C++ Client**: Connects on startup, fetches node definitions.
2.  **Workflow Loop**:
    - **Step A (Sync)**: C++ fetches Node Definitions from Python.
    - **Step B (Edit)**: User creates a graph in C++.
    - **Step C (Send)**: C++ serializes the graph structure to JSON/Binary and sends to Python.
    - **Step D (Run)**: Python parses the graph, executes `run()` on nodes.
    - **Step E (Return)**: Python returns execution status/logs to C++.

## Phase 4: Scene & Visualization
**Goal**: Enable 3D scene manipulation and result playback.

1.  **Scene Data Sync**:
    - Define the `Scene` structure (Entities, Transforms, Components).
    - Implement bi-directional sync or "Editor-Authoritative" scene updates.
2.  **Visualizer**:
    - Implement `Runtime -> Editor` data packets for frame-by-frame visualization.
    - Build a "Playback" controller in the Editor (Play/Pause/Scrub).

## Phase 5: Extension & Polish
**Goal**: Developer experience and deployment.

1.  **Extension API**: Document how to write `MyCustomNode.py` and have it appear in the Editor.
2.  **Asset Pipeline**: Handling meshes, textures, and external resources.
3.  **Deployment**: Packaging the Python env + C++ Editor into a distributable.

## Contribution Guidelines

- **Python-First**: Always define data structures in Python first if they need to be shared. Use the codegen tools to create C++ counterparts.
- **Testing**:
    - Write Python unit tests for Node logic.
    - Write C++ unit tests for UI components and Rendering.
    - Use `tests/` folder for integration tests.
- **Style**:
    - Python: PEP 8.
    - C++: Clang-format (Google style).
