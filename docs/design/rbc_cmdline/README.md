# RBC Commandline Tool

`rbc.exe`是一个命令行工具，开发目录位于 rbc/tool/rbc_cmd/，主要功能是提供一些项目管理和批处理工具，会随着`pip install robocute`一并安装

## Project Management

### Info

- show the brief and detailed info for the project
- `rbc project info --web`: show project info through a web server

### Create

```bash
# 使用 CLI
rbc project create MyRobotProject \
    --name "My Robot Project" \
    --template "uipc" \
    --author "John Doe" \
    --description "A robot simulation project" \
    --git
```

从模板拉取并
  
### Validate

Check if the project is a valid project

### Import Asset

```bash
# 导入单个资产
rbc import asset --project MyRobotProject --asset assets/models/robot.gltf

# 导入目录
rbc import dir --project MyRobotProject --directory assets/ --recursive
```

### Package

`rbc pack/unpack`