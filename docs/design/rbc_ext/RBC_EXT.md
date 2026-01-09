# RBC_EXT 

RBC_EXT的功能是提供python端原生很难提供的功能，比如高性能的计算，异步资源加载等

PythonExtension的目标是为了补足通用python很难完成的工作包括

- 资源的管理，从多种不同的资源加载到通用的运行时资源，方便server和Editor层面同步

在使用的时候，一旦App.is_running()，后台的资源线程会不断处理脚本添加resource的指令，将不同来源的资源封装为一个


## World Interface

- ResourceLoadStatus
- Object
- Entity
- Component
- TransformComponent
- LightComponent
- Resource
- TextureResource
- MeshResource
- MaterialResource
- RenderComponent
- AsyncRequest
- Project
