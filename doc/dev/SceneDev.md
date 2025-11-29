# Scene开发文档

场景和资源同步是整个项目架构的核心部分


## Result Visualization

结果的传递和可视化

结果的定义

TransformAnimSequence
- EntityID
- [timestep, transform]

PhysicsUpdateNode
- input:
  - timestep
  - previous transform
  - Input Scene
- output:
  - time stamp
  - AnimResult

PhysicalAnimUpdateNode
