# 场景设计

Entity::get_component(TypeInfo)

从RTTIType中提取TypeInfo之后，从_components中

BaseObjectType
- None
- Component
- Entity
- Resource
- Custom

## Entity & Component

Entity
- _components

### RenderComponent

RenderComponent是渲染相关的直接业务模块

- ObjectRenderType: Mesh/EmissiveMesh/Procedural
- vector of Material Code
- vector of dep MaterialResource
- tlas idx
- _on_transform_update()
- _mesh_ref: the Mesh Reference

### SkelMeshComponent

The Animatable Skeletal Mesh Component



## Resource System

