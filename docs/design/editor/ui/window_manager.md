# Window Manager

Window Manager是Robocute进行UI管理的核心组件，几乎所有的界面窗口相关的部分都会经过这个API

- Init
  - require plugin_manager
- CleanUp
- createDockableView: 从QWidget生成DockableWidget，将所有权转给DockWidget
- createDockableView: 直接从NativeContribution创建DockWidget