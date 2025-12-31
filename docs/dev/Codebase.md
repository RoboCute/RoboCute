# 代码库基础设施介绍

这里是方便整个RBC代码开发的基础设施介绍，包括如何构建C++和python代码，如何维护文档，整个代码库的基础工具，相关的资源管理等，主要包括

- [PY-FIRST构建系统](codebase/py_first_build.md)：RoboCute的核心构建流程
- [反射与代码生成](codebase/codegen.md)：一套基于typed-python的反射与代码生成框架
- [文档撰写与发布系统](codebase/documentation.md)：我们使用mkdocs构建了一套更新文档的工作流
- [标准库](codebase/standard_lib.md)：我们介绍项目中推荐使用的标准库方案

## 代码的分支结构

以下罗列了当前维护的分支

- master: 主分支，稳定的发布分支，配备自动生成doc page的action，需要小幅度合并提交
- dev: 开发分支，大部分的功能分支的上游分支，随时进行修复与合并，在小版本节点合入master分支
- anim: 动画部分开发分支，定期合入dev分支
- doc: 文档部分开发分支，在每个分支自己维护的文档部分以外，整体的文档构建框架和基本文档的补充分支，定期合入dev分支
- gh-pages: 文档的发布分支

## 代码文件层次

以下介绍了Robocute项目开发中常见目录

- custom_nodes：自定义节点，主要实现各种拓展功能
- docs: 文档原始文件，开发功能最终结束时候记得更新文档
- packages：python第三方库
- rbc: C++源码，包括core/runtime/editor/ext_c/各种plugin/shader等，C++的实现内部存在自己配置的samples和tests，为了避免开发和测试流程过于冗长
- samples: Python测试案例
- src: python源码，包括meta/ext封装/核心robocute/构建脚本scripts等
- test: python测试用例
- thirdparty：C++第三方库源码
- xmake: C++部分的构建脚本