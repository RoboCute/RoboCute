# RBC资源

通用的Mesh/Texture/Physics等资源很难达到最优和最灵活的存取，运行和同步效率，所以RBC引入了自定义资源类型，并支持将多种资源通过懒加载和预烘培引入到RBC Project中

## IResource

IResource构建在IRTTRBasic的基础上，所有资产都会从IResource中继承，从而可以被ResourceSystem以一种统一的方式进行加载，安装，探索依赖和卸载。

```cpp
ConfigResource::~ConfigResource() {
  if (configType.is_zero()) return;
  auto type = rttr_get_type_from_guid(configType);
  type->find_default_ctor().invoke(configData);
  free_aligned(configData, type->alignement());
}

void ConfigResource::SetType(GUID type) {
  if (!configType.is_zero()) {
    assert configData;
    auto oldType = rttr_get_type_from_guid(configType);
    oldType->invoke_dtor(configData);
    free_aligned(configData, oldType->alignment);
  }
  configType = type;
  auto newType = rttr_get_type_from_guid(configType);
  configData = malloc_aligned(newType->size(), newType->alignment());
  newType->find_default_ctor().invoke(configData);
}
```

`auto resource = skr::IResource::SerdeReadWithTypeAs<IResource>(reader)`


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

## ResourceHandle

- load()
- install()
- reset()
- get_loaded()
- get_installed()
- is_null()
- is_guid()
- is_loaded()
- is_installed()
- get_guid()
- get_type()
- get_record()
  - if is_null() return nullptr
  - if is_guid() FindResourceRecord(guid)

## ResourceRequest

- guid的封装
- GetGuid
- GetData
- GetDependencies

## ResourceSystem

- resourceRecords 记录了全局的Resource
- failedRequests
- toUpdateRequests
- serdeBatch
- resourceRecords
- resourceToRecord
- resourceFactories
- ConcurrentQueue of ResourceRequest*: requestsQueue
- ResourceRegistry
- ioService

- RegisterResource
- LoadResource
- FindResourceRecord
- EnqueueResource
- UnloadResource
- FlushResource

- FindFactory
- RegisterFactory
- UnregisterFactory
- GetRegistry

## ResourceRegistry

封装了一套加载Resource的方法，可以处理ResourceRequest按guid查找，后期可以添加单独的数据库

- RequestResourceFile
- CanceRequestFile
- FillRequest
  - fill header
  - vfs
  - resourceUri
  - system
  - factory

## ResourceHeader

- type
- version
- dependencies

- function
- guid
- version
- type
- dependencies
