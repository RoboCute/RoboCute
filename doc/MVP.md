# 最小价值产品(Minimal Valuable Product)

## 核心目标

通过**编辑器驱动的拖放式交互**跑通完整的算法构建和可视化pipeline：

1. **场景准备**: PythonServer加载场景和资源
2. **拖放式连接**: 从编辑器场景视图拖拽Entity到NodeGraph节点输入端口
3. **节点图构建**: 在NodeGraph中连接算法节点（如旋转动画生成器）
4. **执行计算**: PythonServer从Editor中同步初始值（当前场景），执行节点图，生成AnimSequence结果
5. **结果展示**: 在算法节点的Result Dropdown中刷新当前这一次仿真结果的记录，用户选中对应的节点，然后选择Play执行

## Step 1

纯Editor部分功能

- 创建初始场景
- 连接节点图
- 播放结果动画

