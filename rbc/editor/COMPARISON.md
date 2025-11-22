# Comparison: Calculator vs Editor vs Backend

This document explains how the three main components relate to each other.

## Component Overview

| Component | Location | Purpose | Technology |
|-----------|----------|---------|------------|
| **Calculator** | `rbc/calculator/` | Simple standalone calculator demo | Qt + qt_node_editor |
| **Editor** | `rbc/editor/` | Visual editor for backend nodes | Qt + qt_node_editor + HTTP |
| **Backend** | `main.py` + `src/rbc_execution/` | Node execution engine with API | Python + FastAPI |

## Architecture Comparison

### Calculator (Standalone)

```
┌─────────────────────────────────────┐
│         Calculator App              │
├─────────────────────────────────────┤
│ Hardcoded Nodes:                    │
│  - NumberSourceDataModel            │
│  - NumberDisplayDataModel           │
│  - AdditionModel                    │
│  - SubtractionModel                 │
│  - MultiplicationModel              │
│  - DivisionModel                    │
├─────────────────────────────────────┤
│ qt_node_editor                      │
│  - Visual graph editing             │
│  - Immediate execution              │
│  - Save/Load to JSON                │
└─────────────────────────────────────┘
```

**Characteristics**:
- Self-contained, no external dependencies
- Nodes execute immediately on data change
- Fixed set of nodes (must recompile to add more)
- Nodes communicate via shared `DecimalData` objects

### Backend (Server)

```
┌─────────────────────────────────────┐
│      FastAPI Server (Port 8000)     │
├─────────────────────────────────────┤
│ Registered Nodes:                   │
│  - input_number, input_text         │
│  - math_add, math_subtract          │
│  - math_multiply, math_divide       │
│  - text_concat                      │
│  - number_to_text                   │
│  - output, print                    │
├─────────────────────────────────────┤
│ API Endpoints:                      │
│  GET  /nodes                        │
│  GET  /nodes/{type}                 │
│  POST /graph/execute                │
│  GET  /graph/{id}/outputs           │
└─────────────────────────────────────┘
```

**Characteristics**:
- Headless (no GUI)
- RESTful API
- Easy to add new nodes (just Python classes)
- Execution on demand via API call
- Results stored in memory

### Editor (Client + GUI)

```
┌─────────────────────────────────────────┐
│           Editor Application            │
├─────────────────────────────────────────┤
│ UI Layer:                               │
│  - EditorWindow (main window)           │
│  - ExecutionPanel (results display)     │
│  - Node Palette (categorized list)      │
├─────────────────────────────────────────┤
│ Business Logic:                         │
│  - HttpClient (API communication)       │
│  - NodeFactory (dynamic registration)   │
│  - DynamicNodeModel (generic nodes)     │
├─────────────────────────────────────────┤
│ qt_node_editor:                         │
│  - Visual graph editing                 │
│  - Connection management                │
│  - Save/Load scene                      │
└─────────────────────────────────────────┘
          ↓ HTTP/JSON ↓
┌─────────────────────────────────────────┐
│         FastAPI Backend                 │
└─────────────────────────────────────────┘
```

**Characteristics**:
- Client-server architecture
- Nodes defined by backend, not hardcoded
- Execution happens on backend
- Visual feedback in GUI
- Can work with any backend nodes

## Feature Comparison

| Feature | Calculator | Editor | Backend |
|---------|-----------|--------|---------|
| Visual editing | ✅ | ✅ | ❌ |
| Hardcoded nodes | ✅ | ❌ | ❌ |
| Dynamic nodes | ❌ | ✅ | ✅ |
| Immediate execution | ✅ | ❌ | ❌ |
| On-demand execution | ❌ | ✅ | ✅ |
| Save/Load graphs | ✅ | ✅ | ❌ |
| HTTP API | ❌ | Uses it | Provides it |
| Embedded widgets | ✅ | ✅ | N/A |
| Results display | Inline | Multi-mode | JSON |
| Node categories | ✅ | ✅ | ✅ |
| Extensible | Recompile | Backend only | Python code |

## Node Implementation Comparison

### Calculator Node (Hardcoded)

```cpp
// AdditionModel.hpp
class AdditionModel : public NodeDelegateModel {
    // Fixed implementation
    unsigned int nPorts(PortType) const override;
    NodeDataType dataType(...) const override;
    std::shared_ptr<NodeData> outData(PortIndex) override;
    void setInData(...) override;
};
```

**Pros**: Type-safe, fast, optimized
**Cons**: Must recompile to add new nodes

### Backend Node (Python)

```python
# example_nodes.py
@register_node
class MathAddNode(RBCNode):
    NODE_TYPE = "math_add"
    DISPLAY_NAME = "加法"
    CATEGORY = "数学"
    
    @classmethod
    def get_inputs(cls) -> List[NodeInput]:
        return [...]
    
    def execute(self) -> Dict[str, Any]:
        return {"result": a + b}
```

**Pros**: Easy to add/modify, hot-reload possible
**Cons**: Runtime interpretation, no compile-time checks

### Editor Node (Dynamic)

```cpp
// DynamicNodeModel created from JSON metadata
{
  "node_type": "math_add",
  "display_name": "加法",
  "category": "数学",
  "inputs": [...],
  "outputs": [...]
}
```

**Pros**: Adapts to any backend node automatically
**Cons**: Less type safety, generic implementation

## Data Flow Comparison

### Calculator Data Flow

```
Input Widget → NodeData → Connection → NodeData → Output Widget
     ↓                                                ↑
  [User edits value]                          [Immediate display]
```

### Editor + Backend Data Flow

```
1. Editor: Input Widget → Serialize → JSON
2. HTTP POST → Backend
3. Backend: Parse JSON → Create Nodes → Execute Graph
4. Backend: Results → JSON Response
5. Editor: Parse JSON → Display Results
```

## Use Cases

### Calculator

**Best for**:
- Simple demonstrations
- Learning qt_node_editor
- Standalone applications
- Real-time visual feedback

**Example**: Teaching tool for basic math

### Editor + Backend

**Best for**:
- Complex workflows
- Extensible systems
- Remote execution
- Integration with other services
- When node logic is complex

**Example**: 
- Image processing pipeline
- Data analysis workflow
- AI model chaining
- RoboCute rendering pipeline

## Evolution Path

```
Calculator (v1)
    ↓
Editor created (v2) ← You are here
    ↓
Future enhancements:
    - WebSocket for real-time updates
    - Node result overlays
    - Graph templates
    - Collaborative editing
    - Plugin system
```

## Code Reuse

### From Calculator

The editor reuses these patterns:

```cpp
// Node editor setup
DataFlowGraphModel model(registry);
DataFlowGraphicsScene scene(model);
GraphicsView view(&scene);

// Save/Load
scene->save();
scene->load(jsonObject);

// Connection styling
ConnectionStyle::setConnectionStyle(...);
```

### Added in Editor

New functionality:

```cpp
// HTTP communication
HttpClient client;
client.fetchAllNodes(callback);
client.executeGraph(graphDef, callback);

// Dynamic node creation
NodeFactory factory;
factory.registerNodesFromMetadata(metadata, registry);

// Multi-mode display
ExecutionPanel panel;
panel.logMessage(...);
panel.displayExecutionResults(...);
```

## Summary

| Aspect | Calculator | Editor |
|--------|-----------|--------|
| **Complexity** | Simple | Moderate |
| **Flexibility** | Low | High |
| **Dependencies** | Qt only | Qt + Backend |
| **Extensibility** | Recompile | Backend changes |
| **Use Case** | Demo/Learning | Production |
| **Execution** | Local | Remote |

The **Editor** combines the visual advantages of the **Calculator** with the flexibility and extensibility of the **Backend**, creating a powerful system for building and executing node-based workflows.

