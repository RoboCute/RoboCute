# RBCProject

- rbc_project.json 项目入口文件
  - name
  - version
  - meta data ...
- assets：原始图片/gltf/urdf/usd等等DCC导出场景文件
- doc: 项目本身相关的文字信息（设定集，小说等）
- datasets：项目本身的数据集
- mid: The Intermediate Directory
  - resources: asset经过引擎导入之后
  - logs
    - log.db: the sqlite dataset for log
  - out: 默认输出目录
- pretrained: the pretrained weights

