# Testing Guide for RoboCute Editor

This guide walks through testing the end-to-end integration of Python server and C++ editor.

## Prerequisites

1. **Build the Editor**
   ```bash
   xmake build rbc_editor
   ```

2. **Install Python Dependencies**
   ```bash
   pip install fastapi uvicorn
   ```

3. **Prepare Test Mesh**
   - Ensure you have an OBJ file available for testing
   - Default path in samples: `D:/ws/data/assets/models/cube.obj`
   - Or create a simple cube.obj file

## Test 1: Basic Server-Editor Connection

### Step 1: Start Python Server

```bash
python samples/editor_server.py
```

Expected output:
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

### Step 2: Test Server Endpoints

In a separate terminal, test the HTTP endpoints:

```bash
# Test scene state
curl http://127.0.0.1:5555/scene/state

# Test resources
curl http://127.0.0.1:5555/resources/all
```

Expected: JSON responses with scene and resource data

### Step 3: Run C++ Editor

```bash
cd build/windows/x64/release
./rbc_editor.exe
```

Expected output:
```
=== RoboCute Editor ===
Backend: dx
Server: http://127.0.0.1:5555
Registered with server as: editor_001
EditorScene initialized
Editor ready, entering render loop...
Scene updated from server
Entity 1 added to TLAS at index 0
Mesh loaded: 8 vertices, 36 indices
```

Expected behavior:
- Window opens with title "RoboCute Editor"
- Mesh is visible in the viewport
- No errors in console

## Test 2: Verify Scene Synchronization

### Step 1: Observe Python Server

While editor is running, Python server should show:
```
[Status] Connected editors: editor_001
```

### Step 2: Check Scene Updates

The editor polls the server every 100ms. Look for:
- Python server receiving GET requests
- Editor console showing "Scene updated from server" (only on changes)

## Test 3: Camera Controls

With editor running:

1. Press **W** - Camera should move forward
2. Press **S** - Camera should move backward  
3. Press **A** - Camera should move left
4. Press **D** - Camera should move right
5. Press **Q** - Camera should move down
6. Press **E** - Camera should move up
7. Press **Space** - Frame should reset (re-accumulate samples)

Expected: Viewport updates smoothly with each key press

## Test 4: Mesh Loading

### Verify Mesh Load Success

Check editor logs for:
```
Loading mesh from file: D:/ws/data/assets/models/cube.obj
Mesh loaded: X vertices, Y indices
Entity 1 added to TLAS at index 0
```

### Test Different Mesh Files

1. Modify `samples/editor_server.py`:
   ```python
   mesh_path = "path/to/your/mesh.obj"
   ```

2. Restart server and editor

3. Verify new mesh loads correctly

## Test 5: Multiple Entities (Future Test)

Currently the MVP supports single entity. For future testing:

1. Modify Python script to create multiple entities
2. Verify editor displays all entities
3. Check TLAS indices for all instances

## Test 6: Error Handling

### Test Server Disconnection

1. Start editor without server running
2. Expected: Warning about registration failure, but editor continues
3. Start server afterwards
4. Expected: Editor eventually connects and syncs

### Test Invalid Mesh Path

1. In Python server, use non-existent mesh path
2. Expected: Editor logs error but doesn't crash
3. Window stays open with empty scene

### Test Mesh Loading Failure

1. Use invalid OBJ file (corrupted or wrong format)
2. Expected: Editor logs error, continues running
3. Entity should not appear in scene

## Test 7: Performance

### Measure Sync Latency

1. Add logging to Python server:
   ```python
   @app.get("/scene/state")
   def get_scene_state():
       start = time.time()
       result = {...}
       print(f"Request took: {(time.time() - start)*1000:.2f}ms")
       return result
   ```

2. Observe latency in server logs

Expected: < 10ms per request

### Measure Frame Rate

Editor console will show:
- Frame times
- Rendering performance

Expected: 60+ FPS for simple scenes

## Test 8: Graceful Shutdown

### Test Editor Shutdown

1. Close editor window (X button)
2. Expected:
   ```
   Shutting down editor...
   Editor shutdown complete
   ```
3. No crashes or memory leaks

### Test Server Shutdown

1. Press Ctrl+C in server terminal
2. Expected:
   ```
   [Shutdown] Stopping server...
   [Shutdown] Server stopped
   ```
3. Clean exit

## Common Issues and Solutions

### Issue: Editor window is black

**Possible causes:**
- Scene has no entities
- Mesh failed to load
- Camera is inside/behind object

**Solutions:**
1. Check Python server created entity with render component
2. Verify mesh path exists
3. Try moving camera with WASD keys
4. Check editor console for errors

### Issue: "Failed to register with server"

**Possible causes:**
- Server not running
- Port 5555 already in use
- Firewall blocking connection

**Solutions:**
1. Ensure Python server is running first
2. Check `netstat -an | findstr 5555` (Windows) to see if port is open
3. Try different port in both server and editor

### Issue: Mesh not visible

**Possible causes:**
- Mesh path incorrect in Python
- OBJ file format not supported
- Transform places mesh outside camera view

**Solutions:**
1. Print mesh_path in Python to verify
2. Test with known-good OBJ file
3. Adjust transform position in Python (try [0, 0, 3])

### Issue: Editor freezes

**Possible causes:**
- Server request timeout
- Mesh loading blocking
- Graphics driver issue

**Solutions:**
1. Check server is responding to requests
2. Verify mesh file size (very large files may be slow)
3. Update graphics drivers

## Validation Checklist

- [ ] Python server starts without errors
- [ ] HTTP endpoints return valid JSON
- [ ] Editor connects and registers
- [ ] Mesh loads successfully
- [ ] Entity appears in viewport
- [ ] Camera controls work
- [ ] Frame rate is smooth (>30 FPS)
- [ ] Editor shuts down cleanly
- [ ] Server shuts down cleanly
- [ ] No memory leaks detected

## Automated Testing (Future)

To add automated tests:

1. **Unit Tests**: Test individual components
   - HTTP client connection
   - JSON parsing
   - Mesh loading

2. **Integration Tests**: Test server-editor communication
   - Mock HTTP server
   - Verify scene sync
   - Test mesh loading pipeline

3. **Performance Tests**: Measure benchmarks
   - Sync latency
   - Frame time
   - Memory usage

## Reporting Issues

When reporting issues, include:

1. **Environment**:
   - OS version
   - Python version
   - Graphics card

2. **Logs**:
   - Python server console output
   - C++ editor console output
   - Any error messages

3. **Reproduction Steps**:
   - Exact commands run
   - Configuration used
   - Mesh files used

4. **Expected vs Actual**:
   - What you expected to happen
   - What actually happened

## Success Criteria

The implementation is successful when:

✓ Python server starts and serves scene data  
✓ C++ editor connects via HTTP  
✓ Editor loads mesh from file path  
✓ Mesh renders correctly in viewport  
✓ Scene updates synchronize in real-time  
✓ No crashes or memory leaks  
✓ Clean shutdown on both sides

## Next Steps

After successful testing:

1. Test with more complex scenes (multiple entities)
2. Test different mesh file formats
3. Add material synchronization
4. Implement camera sync from server
5. Add interactive features (selection, editing)

