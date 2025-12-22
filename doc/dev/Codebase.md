# 代码库基础设施介绍

这里是方便整个RBC代码开发的基础设施介绍

- doc: 文档
- custom_nodes
- packages
- rbc
  - app
  - core
  - editor
  - ext_c
  - ipc
  - oidn_plugin
  - render_plugin
  - runtime
  - samples
  - shader
  - tests
  - world
  - world_v2
- samples
- src
- test
- thirdparty
- xmake


## py-first标准工作流

Robocute采用python-first的工作流，只在高性能，底层硬件封装和多线程层使用cpp进行加速，所以整体的开发流程也围绕python搭建。整体管理采用`uv`作为包管理工具，任何新增的python库采用`uv add <library>`添加，在新的工作区第一步采用`uv sync`同步基础的工作区组件

### Prepare流程

RBC的C++开发流程需要很多的开源库源码，SDK和工具链，这些二进制工具会在`uv run prepare`的过程中下载到指定的地方，请保证网络通畅。

### Codegen流程

因为Robocute采用大量的python-cpp胶水代码绑定，还有便于网络传输与硬盘加载的序列化-反序列化代码，这些大量的模板代码需要使用codegen进行生成，`uv run gen`的过程会生成所有需要的序列化代码，请务必在这些代码生成之后再进行cpp的编译和开发。

### C++编译

C++主要的编译对象是`rbc_ext_c.pyd`（一个C++和pybind11写成的python拓展）以及`rbc_editor.exe`一个使用Qt作为UI库，LuisaCompute完成Viewport的图形Editor。Robocute使用`xmake`来管理C++的编译流程，在配置好C++编译环境之后直接`xmake`即可

### Install过程

在C++编译完成之后，需要将编译产物install到指定的文件夹目录下，方便python端的打包和分发，同时用stubgen生成必要的存根文件，方便python部分的开发。

`xmake l xmake/install.lua`



## 标准库

### HashMap

其实有个更高效的写法
`auto is_new_elem = resource_factories.try_emplace(type, factory).second;`

如果你不希望在已经有了元素的情况下构建新的元素，那还可以
`resource_factories.try_emplace(type, vstd::lazy_eval([&] { return make_factory(); }))`;
这样 make_factory 只有在需要添加的时候才会调用