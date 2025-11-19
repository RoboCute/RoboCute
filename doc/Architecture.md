# RoboCute Architecture

## Overview

RoboCute adopts a **Python-First, Client-Server** architecture designed to mimic workflows like ComfyUI. The core logic and extensibility reside in Python, while the high-performance visualization and editing happen in a C++ (Qt-based) application.

## System Components

### 1. Python Host (Server & Logic)
The Python process is the entry point and the brain of the operation.
- **Node Registry**: Manages user-defined nodes, their inputs/outputs, and execution logic.
- **Runtime Graph Engine**: Constructs and executes the computational graph based on instructions from the Editor.
- **IPC Server**: Listens for connection from the C++ Editor, handles scene synchronization, and execution requests.
- **Code Generation Source**: Python classes define the schemas (Nodes, Types) which generate C++ bindings and serialization code.

### 2. C++ Editor (Client & Visualization)
The C++ application acts as the frontend interface.
- **RBCStandalone (App)**: The main executable that connects to the Python host.
- **RBCEditorRuntime (Qt)**:
  - **Node Graph Editor**: A visual interface for connecting nodes defined in Python.
  - **Scene Editor**: 3D viewport for scene manipulation.
  - **IPC Client**: Communicates with the Python server (ZeroMQ/HTTP/WebSocket etc.) to fetch node definitions and send execution graphs.
  - **Visualizer**: Renders the results sent back from the Python runtime (Replay, Debug data).

### 3. Shared Core & Runtime (C++/Python Bridge)
- **RBCCore**:
  - `object`: Serialization (JSON/Binary) and Reflection. Essential for syncing data between Python and C++.
  - `platform`: OS abstractions.
  - `type`: Common types (Vector, Matrix, Color) shared across languages.
  - `math`: High-performance math library.
- **RBCRuntime**:
  - `scene`: The scene graph data structure.
  - `resource`: Asset management.
- **RBCExt (Pybind11/Nanobind)**:
  - Exposes C++ Runtime and Core functionality to Python.
  - Allows Python to manipulate the C++ scene state directly if needed.

## Data Flow & Workflow

1.  **Startup**: `python main.py` starts the Python Server and launches the C++ Editor process.
2.  **Registration**: Python scans for Node definitions and builds a `NodeManifest`.
3.  **Handshake**: C++ Editor connects to Python Server and requests the `NodeManifest`.
4.  **Editing**:
    - User manipulates the **Editor Node Graph** in C++ using the definitions provided by Python.
    - User edits the **Scene** (3D objects, properties).
5.  **Execution Trigger**:
    - User presses "Run".
    - C++ serializes the Node Graph and Scene Parameters.
    - Sends `ExecutionRequest` to Python.
6.  **Runtime Execution**:
    - Python receives the graph.
    - Builds a **Runtime Graph**.
    - Executes logic (Solver, AI, Simulation).
    - Collects `ExecutionResults` (Trajectory, Sensor Data, Logs).
7.  **Visualization**:
    - Python sends `ExecutionResults` back to C++.
    - C++ updates the viewport and timelines to visualize the outcome.

## Module Structure

- **rbc/**: C++ Source
  - **core/**: Foundation (Memory, Math, Serde).
  - **runtime/**: Simulation engine components.
  - **editor_runtime/**: Qt-based UI components.
  - **tests/**: C++ Unit tests.
  - **util/**: Utility helpers.
- **scripts/**: Python Source
  - **py/**: Codegen logic, Runtime engine, Node definitions.
  - **xmake/**: Build scripts.
- **rbc_ext/**: Binding layer.
