# RoboCute编辑器

这篇文档旨在向Robocute的使用者介绍RBC Editor，一个Robocute自研的场景编辑器的设计思路和使用方法，如果您希望进一步了解源码和开发架构的设计，请参考[RBC Editor开发文档](../dev/EditorDev.md)

和游戏引擎与工业软件的重度编辑器不同，RoboCute的编辑器更类似于一个Debugger或者Inspector，旨在帮助完成一些用代码实现非常繁琐的参数配置，流程配置，提前预览，结果而可视化的部分。

Editor启动流程

- 开始
- 从默认安装位置加载配置
  - 之前的缓存配置
    - 上一次打开的Project
    - 打开过的Project
  - 本地机器平台相关配置
  - User相关配置
- 从开启选项决定是否打开project
- 决定是否默认打开服务器监听，监听服务器则无法打开本地project
- 监听后端链接
  - 加载服务器Project
  - 加载服务器Scene
- 手动打开project
  - 加载本地Project
  - 加载本地Scene
- 加载Project之后，从Project加载默认Scene
  - 展示支持加载的Scene
EditorContext
- 打开的Project
- 打开的EditorScene