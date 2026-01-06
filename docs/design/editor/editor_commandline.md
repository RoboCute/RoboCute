# Editor Commandline

Editor自己需要一些启动参数，方便开发和调试

```
RoboCuteEditor.exe [options]

Options:
  --qml-dev              启用QML热重载模式（从源码目录加载QML）
  --qml-source <path>    指定QML源码根目录（默认：./rbc/editor/runtime/qml）
  --plugin-dir <path>    额外的插件搜索目录
  --no-plugins           禁用外部插件加载
  --layout <name>        使用指定的布局配置
  --connect <host:port>  自动连接到Python Server
  --headless             无头模式（用于测试）
  --verbose              详细日志输出

示例：
  # 开发模式，启用QML热重载
  RoboCuteEditor.exe --qml-dev --qml-source D:/ws/repos/RoboCute-repo/RoboCute/rbc/editor/runtime/qml
  
  # 加载自定义插件目录
  RoboCuteEditor.exe --plugin-dir ./custom_plugins
  
  # 无头模式运行测试
  RoboCuteEditor.exe --headless --no-plugins
```