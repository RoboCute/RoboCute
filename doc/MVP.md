# 最小价值产品(Minimal Valuable Product)

旨在通过最直接的方式跑通pipeline，包括

- PythonServer加载场景和资源
- PythonServer自定义命令节点（这个例子是一个简单的绕中心点旋转的Transform动画）
- Editor连接
- Editor编辑场景参数
- Editor编辑节点图
- Editor发起执行命令
- PythonServer同步场景和参数
- PythonServer执行RuntimeNodeGraph和对应python完成的方法
- Editor通过查询获得执行状态
- 执行完毕生成节点定义的Result(是一系列Timestep和对应的Transform构成的AnimSequence)
- 将ResultAnimSequence同步到Editor
- Editor执行播放ResultAnimSequence查看结果
