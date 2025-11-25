# RoboCute

- rbc_editor: The Editor via Qt and LuisaCompute
- rbc_ext: wrap the editor 
- doc: documentatino

## How to Get Started

- see [[BUILD.md]]


## Architecture

- src
  - rbc_meta: python和cpp的通用codegen入口，元信息结构定义
  - rbc_ext: cpp pybind的封装层
  - robocute: python的核心功能


## Python API 设计示例

### 场景脚本示例（无Editor）

```python
import robocute as rbc

# 创建场景
scene = rbc.Scene()
scene.start()
# Load resources
mesh_id = scene.load_mesh("models/robot.obj", priority=rbc.LoadPriority.High)
material_id = rbc.create_default_material(
    scene.resource_manager,
    name="metal",
    metallic=0.9,
    roughness=0.3
)
# 纯代码创建场景
ground = scene.create_entity("Ground")
scene.add_component(ground, rbc.TransformComponent(position=[0, 0, 0]))
scene.add_component(ground, rbc.MeshComponent(mesh="plane.obj"))
scene.add_component(ground, rbc.PhysicsComponent(body_type="static"))
# Create entity with render component
robot = scene.create_entity("Robot")
scene.add_component(robot.id, "transform", rbc.TransformComponent(position=[0, 1, 0]))
scene.add_component(robot.id, "render", rbc.RenderComponent(
    mesh_id=mesh_id,
    material_ids=[material_id]
))

# 加载节点图（可以是用Editor GUI编辑保存的）
graph = rbc.load_node_graph("path_planning.json")

# 执行仿真
for frame in range(1000):
    scene.execute_node_graph(graph)
    scene.update(dt=0.016)  # 60 FPS

# 保存场景（供下次使用或Editor查看）
scene.save("final_scene.rbc")
```

### 带Editor的开发流程

```python
import robocute as rbc

# 创建场景
scene = rbc.Scene()

# 可选：加载之前保存的场景
# scene.load("my_scene.rbc")

# 启动Editor服务（可选）
editor_service = rbc.EditorService(scene)
editor_service.start(port=5555)

# 启动Editor GUI（可选）
editor_process = rbc.launch_editor()  # subprocess启动RBCEditor.exe

# 主循环
try:
    while True:
        # 处理Editor命令
        editor_service.process_commands()
        
        # 更新仿真
        scene.update(dt=0.016)
        
        # 如果有节点图在执行，推送仿真状态到Editor
        if scene.is_executing():
            editor_service.push_simulation_state()
            
except KeyboardInterrupt:
    print("Shutting down...")
    
finally:
    # 保存场景
    scene.save("auto_save.rbc")
    
    # 关闭Editor服务
    editor_service.stop()
    editor_process.terminate()
```

---