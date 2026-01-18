# Editor开发文档

这篇文档主要描述RBC Editor的代码开发架构，旨在向希望了解的开发者说明Editor部分的源码和资源如何组织，样式如何调整，如何运行测试等开发过程中相关的设计，如果您只是想要了解和使用RBC Editor，请参考[RBC Editor设计文档](../design/Editor.md)

## Plugin架构

## QML组件样式

对于基础的视觉元素，rbc采用Qt的QML进行加载和统一的样式管理，由于编辑器需要支持比较复杂的组件如Viewport或者NodeEditor，从架构上必须以C++为主，所以QML元素主要用于小组件和插件。

