# Robocute Project




config
- app-level config: 主要在一次安装中进行的配置
  - runtime path: 安装可执行文件的目录
  - shader path: shader搜索和加载地址
  - default project path: 默认场景地址，禁止原地保存
- project-level config: 需要打开一个具体的project才能确定的配置
  - meta data: project的名字、作者、版权、描述等等
  - scene_path: 场景地址
  - resource_path: 资源地址
  - asset_path: 为了方便导入的资产地址，可以直接带着分享
  - intermediate_path: 中间产物地址