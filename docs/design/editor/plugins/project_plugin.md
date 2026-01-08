# Project Explorer

项目浏览器，用来直接在Editor中浏览一个rbc project内有什么资源

- 数据源来自[IProjectService](../services/Project.md)
- ViewContribution
  - Project Explorer：一个dockable window，停靠在Project旁边，浏览Project中的资产信息，附带Project工具的入口
  - Project Setting Panel: 在Menu中打开，设置Project基本信息
- Menu Contribution
  - Project Setting：Project Setting Panel入口
