# RoboCute Project

## Create Project

```py
import robocute as rbc

project = rbc.Project.create(
    path="D:/Projects/MyRobotProject",
    template="uipc",
    name="My Robot Project",
    author="John Doe",
    description="A robot simulation project",
    version="0.1.0"
)
```

## Import Asset

```python
import robocute as rbc

project = rbc.Project.load("D:/Projects/MyRobotProject")
importer = project.import_manager()

# 导入单个资产
result = importer.import_asset("assets/models/robot.gltf")
print(f"Imported {len(result['resources'])} resources")

# 导入目录
result = importer.import_directory("assets", recursive=True)
```