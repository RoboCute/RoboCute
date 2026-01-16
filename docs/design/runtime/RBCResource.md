# RBC资源

通用的Mesh/Texture/Physics等资源很难达到最优和最灵活的存取，运行和同步效率，所以RBC引入了自定义资源类型，并支持将多种资源通过懒加载和预烘培引入到RBC Project中

## 外界期待的使用方法

对我这个Component来说，刚拿出来的时候只知道我会持有一个Resource的guid，然后我拿着这个guid去问ResourceSystem拿到了一个Future或者Handle啥的，之后我自己Update的时候就不停地拿这个Handle来查它有没有installed/loaded，一旦资源加载完就正式进入流程，在流程内部就不再检查这个资源是否valid，默认都是加载完毕的状态。

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


MeshResource是最特殊和复杂的Resource之一，一端连接着引擎的功能模块，一段连接着图形模块

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

- get_or_create_property_buffer
- 

#### MeshResource::create_empty

指定
- path
- file_offset
- submesh_offset
- vertex_count
- triangle_count
- uv_count
- contained_normal
- contained_tangent

重置
- origin_mesh：the ref-counted original MeshResource
- device_res

类型
- device_res->resource_type 
  - TransformingMesh
  - Buffer
  - Image
  - Mesh
  - SparseImage
  

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
