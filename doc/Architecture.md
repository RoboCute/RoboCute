# RoboCute Architecture

- RBCCore
  - object: 提供序列化和类型反射
  - platform: 提供一些基本的跨平台能力
  - type: 提供一些通用的类型接口和简单的容器
  - math: 提供带加速的数学库
  - thread：基本的线程封装
- RBCUtil层：包含工具函数实现和大量生成代码的接口
- RBCRuntime: 接入LuisaCompute等动态库，实现渲染/动画/物理等核心业务
  - scene: node based场景树
  - resource: 资源层封装
- RBCEditorRuntime: 接入Qt，实现编辑功能
- RBCEditorStandalone: app层
- RBCExt: Pyind Extension层