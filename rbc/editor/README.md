# RoboCute Node Editor

A Qt-based visual node editor that integrates with the RoboCute FastAPI backend node system.

## Features

- **Dynamic Node Loading**: Query and register node types from the FastAPI backend
- **Visual Graph Editing**: Drag-and-drop node creation and connection using qt_node_editor
- **Graph Execution**: Execute graphs via HTTP API and display results
- **Multi-Mode Display**: 
  - Console log panel
  - Tree-based results viewer
  - Node overlay for results (planned)
- **Save/Load**: Persist graphs to JSON files
- **Real-time Status**: Connection status indicator and execution progress

## Building

### Prerequisites

- Qt 6 (with Network module)
- xmake build system
- qt_node_editor (included in thirdparty)

### Build Commands

```bash
# From the project root
xmake build editor

# Run the editor
xmake run editor
```

## Usage

### 1. Start the Backend Server

First, start the FastAPI backend server:

```bash
python main.py
```

The server will start at `http://127.0.0.1:8000` by default.

### 2. Launch the Editor

```bash
xmake run editor
```

### 3. Verify Connection

- Check the connection status indicator in the toolbar
- Green = Connected to backend
- Red = Disconnected

If disconnected, go to `Tools > Server Settings` to configure the backend URL.

### 4. Create a Node Graph

1. **Browse Nodes**: The left panel shows available node types organized by category
2. **Add Nodes**: Right-click in the graph area and select a node from the menu
3. **Configure Nodes**: Click on a node to edit its parameters in the embedded widget
4. **Connect Nodes**: Drag from an output port to an input port to create connections
5. **Execute**: Click the "Execute" button or press F5 to run the graph

### 5. View Results

Results are displayed in multiple ways:

- **Console Tab**: Shows execution log messages
- **Results Tab**: Tree view of all node outputs
- Status bar shows execution summary

## Menu Options

### File Menu

- **New** (Ctrl+N): Create a new graph
- **Save** (Ctrl+S): Save graph to JSON file
- **Open** (Ctrl+O): Load graph from JSON file
- **Exit** (Ctrl+Q): Close the application

### Execute Menu

- **Execute Graph** (F5): Run the current graph

### Tools Menu

- **Refresh Nodes** (F5): Reload node types from backend
- **Server Settings**: Configure backend server URL

## Architecture

### Components

- **HttpClient**: Handles HTTP communication with the FastAPI backend
- **NodeFactory**: Dynamically creates node models from backend metadata
- **DynamicNodeModel**: Generic node implementation that adapts to any node type
- **EditorWindow**: Main application window with graph editor
- **ExecutionPanel**: Results and console display

### Data Flow

1. Editor queries `/nodes` API to get available node types
2. NodeFactory creates DynamicNodeModel instances for each type
3. User builds graph visually
4. On execute, graph is serialized to JSON matching backend schema
5. POST to `/graph/execute` triggers backend execution
6. GET `/graph/{id}/outputs` retrieves results
7. Results displayed in UI

## Backend API Integration

The editor communicates with these FastAPI endpoints:

- `GET /health` - Health check and connection status
- `GET /nodes` - List all available node types
- `GET /nodes/{type}` - Get details for a specific node type
- `POST /graph/execute` - Execute a graph
- `GET /graph/{id}/outputs` - Retrieve execution results

## File Format

Graphs are saved as JSON files containing:

- Node instances with IDs, types, and input values
- Connections between nodes
- Qt node editor layout information

Example structure:

```json
{
  "nodes": [
    {
      "id": "1",
      "model": {
        "node_type": "input_number",
        "input_values": {"value": 10}
      }
    }
  ],
  "connections": [...]
}
```

## Troubleshooting

### Cannot Connect to Backend

- Ensure the backend server is running (`python main.py`)
- Check the server URL in `Tools > Server Settings`
- Verify the backend is accessible (try http://127.0.0.1:8000/docs in a browser)

### No Nodes Available

- Refresh nodes with `Tools > Refresh Nodes`
- Check the console panel for error messages
- Ensure example nodes are loaded in the backend

### Execution Fails

- Check the console panel for detailed error messages
- Verify all required node inputs are connected or have values
- Ensure node connections are valid (matching input/output types)

## Development

### Adding New Features

The codebase is organized as follows:

- `main.cpp` - Application entry point
- `EditorWindow.*` - Main window and graph management
- `HttpClient.*` - Backend API communication
- `DynamicNodeModel.*` - Dynamic node implementation
- `NodeFactory.*` - Node creation and registration
- `ExecutionPanel.*` - Results display

### Extending the Editor

To add new features:

1. **Custom Node Widgets**: Modify `DynamicNodeModel::createWidgetForInput()`
2. **New Display Modes**: Extend `ExecutionPanel`
3. **Additional API Calls**: Add methods to `HttpClient`

## License

Part of the RoboCute project.

