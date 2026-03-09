# Robocute Project

RBC存在工作项目的概念，需要本地的资源和缓存，需要导入与导出，这虽然不像USD那样可以移动，但是给效率，可用性与扩展性带来了便利。尽管project会带有一定的spec，我们还是会兼容标准的python project，让robocute能插入嵌入任何现有的python工作流。

Project-Level Development

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
