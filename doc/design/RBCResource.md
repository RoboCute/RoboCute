# RBC资源

通用的Mesh/Texture/Physics等资源很难达到最优和最灵活的存取，运行和同步效率，所以RBC引入了自定义资源类型，并支持将多种资源通过懒加载和预烘培引入到RBC Project中

## IResource

IResource构建在IRTTRBasic的基础上，所有资产都会从IResource中继承，从而可以被ResourceSystem以一种统一的方式进行加载，安装，探索依赖和卸载。

### MeshResource

- string name
- vector: sections
- vector: primitives
- buffer bins
- vector of AsyncResource of MaterialResource
- install_to_vram
- install_to_ram
- pointer to RenderMesh


MaterialResource

MaterialTypeResource

## ResourceSystem
