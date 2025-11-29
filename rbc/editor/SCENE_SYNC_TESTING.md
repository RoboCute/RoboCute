# Scene Synchronization Testing Guide

This document provides step-by-step instructions for testing the scene synchronization feature between the Python server and Qt Editor.

## Prerequisites

1. **Build the Editor:**
   ```bash
   xmake build rbc_editor
   ```
   
   Or regenerate Visual Studio project:
   ```bash
   xmake project -k vsxmake2022
   ```

2. **Python Dependencies:**
   Ensure you have the RoboCute Python package and FastAPI installed:
   ```bash
   pip install fastapi uvicorn
   ```

## Test Procedure

### Step 1: Start the Python Server

1. Navigate to the project root
2. Run the editor server example:
   ```bash
   python samples/editor_server.py
   ```

3. **Expected Output:**
   ```
   ============================================================
   RoboCute Editor Server Example
   ============================================================
   
   [1] Creating scene...
       Scene created and started
   
   [2] Loading mesh resource...
       Mesh registered with ID: 1
   
   [3] Creating scene entities...
       Created entity: Robot (ID: 1)
       Added transform component
       Added render component
   
   [4] Starting Editor Service...
       Editor Service started on port 5555
       Endpoints available:
         - GET  http://127.0.0.1:5555/scene/state
         - GET  http://127.0.0.1:5555/resources/all
         - POST http://127.0.0.1:5555/editor/register
   
   [5] Server is running...
       You can now start the C++ editor (editor.exe)
   ```

4. **Verify Server Endpoints:**
   Open browser and check:
   - http://127.0.0.1:5555/scene/state - Should return scene JSON
   - http://127.0.0.1:5555/resources/all - Should return resources JSON

### Step 2: Start the Qt Editor

1. Run the editor executable:
   ```bash
   ./build/windows/x64/release/rbc_editor.exe
   ```

2. **Expected Behavior:**
   - Editor window opens with Qt UI
   - Status bar shows "Connected to scene server"
   - Scene Hierarchy panel (left) displays entities from server
   - 3D Viewport (center) renders the mesh
   - Details panel (right) shows "Select an entity to view its properties"

### Step 3: Verify Scene Hierarchy

**Check the Scene Hierarchy Panel (Left Side):**

Expected tree structure:
```
Scene Root
└── Robot (ID: 1)
    ├── Transform
    └── Render (Mesh: 1)
```

**Actions:**
- Expand/collapse tree nodes
- Click on the Robot entity

### Step 4: Verify Details Panel

**After selecting Robot entity in hierarchy:**

Expected details display:
```
Transform Component:
  Position: [0.000, 1.000, 3.000]
  Rotation: [0.000, 0.000, 0.000, 1.000] (Quaternion)
  Scale:    [1.000, 1.000, 1.000]

Render Component:
  Mesh ID:     1
  Mesh Path:   D:/ws/data/assets/models/bunny.obj (or your mesh path)
  Material:    Default (OpenPBR)
```

### Step 5: Verify 3D Viewport Rendering

**3D Viewport (Center):**

Expected rendering:
- Mesh appears at position (0, 1, 3)
- Default PBR material applied
- Area light illuminating the scene
- Camera positioned at (0, 1, 5)

**Camera Controls:**
- W/S: Pitch camera up/down
- A/D: Yaw camera left/right
- Verify smooth camera movement
- Frame accumulation resets when camera moves

### Step 6: Test Real-Time Synchronization

**Modify the Python script while running:**

1. Open `samples/editor_server.py` in editor
2. After line 78 (where render component is added), add:
   ```python
   # Dynamically update position
   import threading
   def update_position():
       import time
       for i in range(100):
           time.sleep(0.5)
           # Update entity position
           transform = entity.get_component("transform")
           transform.position = [i * 0.1, 1.0, 3.0]
           scene.add_component(entity.id, "transform", transform)
   
   threading.Thread(target=update_position, daemon=True).start()
   ```

3. Restart the Python server
4. Start the editor
5. **Expected:** Mesh should move smoothly along X-axis in the viewport

**Or test interactively in Python console:**
```python
# While server is running, in another terminal:
import requests
response = requests.get("http://127.0.0.1:5555/scene/state")
print(response.json())
```

### Step 7: Test Connection Resilience

1. **Stop Python server** (Ctrl+C)
2. **Expected Editor behavior:**
   - Status bar changes to "Disconnected from scene server"
   - Scene remains displayed (last known state)
   - No crashes

3. **Restart Python server**
4. **Expected Editor behavior:**
   - Status bar changes to "Connected to scene server"
   - Scene updates to latest state
   - Automatic reconnection

## Success Criteria Checklist

- [ ] Scene Hierarchy displays entities from server
- [ ] Tree structure matches server scene
- [ ] Entity selection works
- [ ] Details panel shows correct entity properties
- [ ] Transform values are accurate
- [ ] Render component information is correct
- [ ] Mesh path is displayed
- [ ] 3D Viewport renders mesh at correct position
- [ ] Camera controls (WASD) work smoothly
- [ ] Sync updates occur within 100ms
- [ ] Status bar shows connection status
- [ ] No crashes on scene changes
- [ ] Reconnection works after server restart
- [ ] Multiple entities supported
- [ ] Resource metadata correctly displayed

## Troubleshooting

### Issue: Editor can't connect to server
**Solutions:**
- Verify Python server is running on port 5555
- Check firewall settings
- Ensure no other application is using port 5555
- Try explicitly: `window.startSceneSync("http://127.0.0.1:5555")`

### Issue: Mesh not displaying in viewport
**Solutions:**
- Check mesh file path in Python script exists
- Verify OBJ file is valid
- Check editor logs for loading errors
- Ensure mesh is assigned to entity render component
- Try adjusting camera position with WASD

### Issue: Blank/black viewport
**Solutions:**
- Scene may be empty (no entities with render components)
- Check server logs for entity creation
- Verify TLAS is built (check editor logs: "TLAS ready = true")
- Try adjusting camera position

### Issue: Scene Hierarchy is empty
**Solutions:**
- Verify server is returning scene data: `curl http://127.0.0.1:5555/scene/state`
- Check for JSON parsing errors in editor logs
- Ensure entities have valid IDs and names

### Issue: Details panel shows wrong data
**Solutions:**
- Click on entity in hierarchy again
- Verify resource IDs match between scene and resources
- Check that mesh_id in render component corresponds to actual resource

## Performance Benchmarks

Expected performance metrics:
- **Sync latency:** < 10ms per request
- **Sync frequency:** 10 Hz (100ms intervals)
- **Heartbeat frequency:** 1 Hz (1s intervals)
- **Rendering FPS:** 60+ for simple scenes
- **Scene update latency:** < 100ms from Python change to editor display

## Debug Logging

Enable verbose logging by adding to editor startup:
```cpp
luisa::log_level_verbose();
```

Or in Python:
```python
import logging
logging.basicConfig(level=logging.DEBUG)
```

Check logs for:
- `[EditorScene]` - Scene updates, mesh loading
- `[SceneSync]` - JSON parsing, entity tracking
- `[HttpClient]` - Network requests/responses
- `[SceneSyncManager]` - Sync lifecycle events

## Next Steps

After successful basic testing:
1. Test with multiple entities
2. Test with different mesh formats
3. Test with large scenes (100+ entities)
4. Add material synchronization
5. Add camera sync
6. Add interactive transform editing
7. Add entity creation/deletion from editor

## Known Limitations (Current Implementation)

1. **Rotation:** Quaternion rotation not fully applied in transform matrix
2. **Materials:** Only default OpenPBR material used
3. **Multiple meshes:** Not fully tested with many entities
4. **Edit from editor:** One-way sync only (server → editor)
5. **Windows only:** WinHTTP limits to Windows platform (Note: HttpClient now uses Qt, so cross-platform)

## References

- Main implementation plan: `migrate.plan.md`
- Python server example: `samples/editor_server.py`
- Sample sync_editor: `rbc/samples/sync_editor/`
- Architecture doc: `doc/Editor.md`

