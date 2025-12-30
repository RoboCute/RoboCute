# RBC Animation Module

RBC动画模块设计

## Skinning

- 首先创建一个运行时MeshResource `skinning_mesh=world::create_object<world::MeshResource>()`
- 然后从静态Mesh创建 `skinning_mesh->create_as_morphing_instance(static_mesh)`
- 初始化GPU资源 `skinning_mesh->init_device_resource`
- 对这样创建的MeshResource，可以`skinning_mesh->origin_mesh()`获取静态资产
- mesh_data
- `device_mesh = skinning_mesh->device_transforming_mesh()`
- 通过`device_mesh->host_data_ref()`拿到CPU数据
- 然后自己用`16 * vert_id` 拿到pos数据并替换……

MeshPack
- data_view
- data
- mutable_data
- submesh_indices
- mesh

