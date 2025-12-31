# Scene Management

(WIP)

Robocute内置了一个`Entity-Component`的场景管理策略，所以外部资源都需要通过importer“导入”之后才能在内部场景中使用，这样的设计是为了不同数据来源彼此之间的不兼容，以及为了实现自身渲染-物理-动画-网络同步等功能的时候足够高效

