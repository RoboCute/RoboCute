# Robocute Extensions

新版Extensions主要分为rbc_ext_c和lcapi_c，位于rbc/extensions/文件夹下，ext_c主要从world_interface代码生成出来world相关的接口，而lcapi_c则手动导出了lcpy相关的类型，函数和内部方法。

例子
- samples/app_graphics_scene.py
- samples/app_object_move.py

使用

```bash
xmake f -m debug -c
xmake l xmake/install.lua debug uv 
uv run samples/app_graphics_scene.py -p <path_to_your_project>
```

当前已经支持从外部pip install，如再rbc-project-default目录下

```bash
uv sync
uv pip install -e ../RoboCute # 手动安装RoboCute
uv run main.py -p .
```