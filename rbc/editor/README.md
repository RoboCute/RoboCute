# RoboCute Editor

A real-time 3D editor that synchronizes with Python server to display and render scenes.

## Overview

The RoboCute editor is a C++ application that connects to a Python server via HTTP REST API to receive scene information and display loaded meshes in a rendering window. The editor follows a Python-first architecture where the Python server is the single source of truth for scene data.

## Architecture

```
Python Server (Port 5555)
  ├─ FastAPI HTTP REST API
  ├─ Scene Management
  └─ Resource Management
      
      ↕ HTTP (JSON)
      
C++ Editor
  ├─ HTTP Client (WinHTTP)
  ├─ Scene Synchronization
  ├─ Mesh Loading (OBJ files)
  └─ Real-time Rendering (LuisaCompute)
```

## Components

### HTTP Client (`http_client.h/cpp`)
- Windows HTTP client using WinHTTP API
- Communicates with Python server via REST endpoints
- Endpoints:
  - `GET /scene/state` - Fetch scene entities and transforms
  - `GET /resources/all` - Fetch resource metadata
  - `POST /editor/register` - Register editor with server
  - `POST /editor/heartbeat` - Keep-alive heartbeat

### Scene Synchronization (`scene_sync.h/cpp`)
- Manages local mirror of Python scene
- Parses JSON responses from server
- Tracks entities, transforms, and resource metadata
- Detects changes and triggers updates

### Editor Scene (`editor_scene.h/cpp`)
- Loads meshes from file paths using `MeshLoader`
- Manages TLAS (Top-Level Acceleration Structure) for ray tracing
- Updates entity transforms dynamically
- Integrates with rendering pipeline

### Main Editor (`main.cpp`)
- Initializes rendering device and window
- Polls server for scene updates every 100ms
- Renders scene using LuisaCompute rendering pipeline
- Handles keyboard input for camera control

## Building

### Prerequisites
- CMake or XMake
- C++20 compiler (MSVC on Windows)
- LuisaCompute
- RoboCute runtime and world modules

### Build with XMake

```bash
xmake build rbc_editor
```

The executable will be in `build/windows/x64/release/rbc_editor.exe`

## Usage

### 1. Start Python Server

First, start the Python server with a scene:

```bash
python samples/editor_server.py
```

This will:
- Create a scene with a robot entity
- Load a mesh resource
- Start HTTP server on port 5555
- Wait for editor connection

### 2. Run C++ Editor

In a separate terminal, run the editor:

```bash
# Default (DirectX backend, default server)
./build/windows/x64/release/rbc_editor.exe

# Specify backend and server
./build/windows/x64/release/rbc_editor.exe dx http://127.0.0.1:5555
```

Arguments:
- `argv[1]`: Graphics backend (`dx` or `vk`, default: `dx`)
- `argv[2]`: Server URL (default: `http://127.0.0.1:5555`)

### 3. Controls

- **WASD**: Move camera horizontally
- **Q/E**: Move camera vertically
- **Space**: Reset frame accumulation

## API Communication

### Scene State Format (JSON)

```json
{
  "success": true,
  "scene": {
    "name": "Untitled",
    "entities": [
      {
        "id": 1,
        "name": "Robot",
        "components": {
          "transform": {
            "position": [0.0, 1.0, 3.0],
            "rotation": [0.0, 0.0, 0.0, 1.0],
            "scale": [1.0, 1.0, 1.0]
          },
          "render": {
            "mesh_id": 123,
            "material_ids": []
          }
        }
      }
    ]
  }
}
```

### Resource Metadata Format

```json
{
  "success": true,
  "resources": [
    {
      "id": 123,
      "type": 1,
      "path": "D:/ws/data/assets/models/cube.obj",
      "state": 2,
      "size_bytes": 12345
    }
  ]
}
```

## Development

### Adding New Features

1. **Custom Components**: Add parsing in `scene_sync.cpp`
2. **New Resource Types**: Extend `EditorScene` loading logic
3. **Additional Controls**: Modify keyboard callback in `main.cpp`

### Debugging

- Check Python server logs for HTTP requests
- Editor logs connection status and scene updates
- Use `LUISA_INFO/WARNING/ERROR` for C++ logging

## Troubleshooting

### Editor can't connect to server
- Ensure Python server is running on port 5555
- Check firewall settings
- Verify server URL in command line args

### Mesh not displaying
- Check mesh file path in Python script
- Ensure OBJ file exists and is valid
- Check editor logs for loading errors
- Verify mesh is assigned to entity render component

### Blank/black window
- Scene may be empty (no entities)
- Check server logs for entity creation
- Verify TLAS is built (check editor logs)
- Try adjusting camera position with WASD keys

## Examples

### Minimal Python Server

```python
import robocute as rbc
import time

# Create scene
scene = rbc.Scene()
scene.start()

# Load mesh
mesh_id = scene.load_mesh("path/to/mesh.obj")

# Create entity
entity = scene.create_entity("MyObject")
scene.add_component(entity.id, "transform", 
    rbc.TransformComponent(position=[0, 0, 3]))
scene.add_component(entity.id, "render",
    rbc.RenderComponent(mesh_id=mesh_id))

# Start editor service
service = rbc.EditorService(scene)
service.start(port=5555)

# Keep running
try:
    while True:
        time.sleep(0.1)
except KeyboardInterrupt:
    service.stop()
    scene.stop()
```

## Future Enhancements

- [ ] WebSocket support for real-time updates
- [ ] Multiple mesh support per entity
- [ ] Material synchronization from server
- [ ] Camera position/rotation sync
- [ ] Interactive object selection
- [ ] Property editing GUI
- [ ] Animation playback
- [ ] Performance metrics display

## License

Part of the RoboCute project.

