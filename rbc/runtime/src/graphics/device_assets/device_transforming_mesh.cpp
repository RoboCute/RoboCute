#include <rbc_graphics/device_assets/device_transforming_mesh.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/scene_manager.h>

namespace rbc {
DeviceTransformingMesh::DeviceTransformingMesh() {}
DeviceTransformingMesh::~DeviceTransformingMesh() {
    if (_render_mesh_data) {
        auto inst = AssetsManager::instance();
        if (inst) {
            inst->scene_mng()->mesh_manager().emplace_unload_mesh_cmd(_render_mesh_data);
            if (_last_vertex_heap_idx != ~0u)
                inst->scene_mng()->bindless_allocator().deallocate_buffer(_last_vertex_heap_idx);
        }
    }
}
void DeviceTransformingMesh::async_load(RC<DeviceMesh> origin_mesh, bool init_buffer, bool init_last_vertex) {
    _origin_mesh = std::move(origin_mesh);
    auto inst = AssetsManager::instance();
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push([init_buffer, init_last_vertex, this_shared = RC{this}](LoadTaskArgs const &args) {
        auto inst = AssetsManager::instance();
        auto ptr = static_cast<DeviceTransformingMesh *>(this_shared.get());
        if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
        ptr->_gpu_load_frame = args.load_frame;
        ptr->_render_mesh_data = inst->scene_mng()->mesh_manager().make_transforming_instance(
            inst->scene_mng()->bindless_allocator(), args.cmdlist,
            ptr->_origin_mesh->mesh_data());
        if (init_last_vertex) {
            ptr->_last_vertex = args.device.create_buffer<float3>(ptr->_render_mesh_data->meta.vertex_count);
            ptr->_last_vertex_heap_idx = inst->scene_mng()->bindless_allocator().allocate_buffer(ptr->_last_vertex);
        }
        if (init_buffer) {
            auto pos_buffer = ptr->_origin_mesh->mesh_data()->pack.data.view(0, ptr->_last_vertex.size_bytes() / sizeof(uint)).as<float3>();
            args.cmdlist
                << ptr->_render_mesh_data->pack.data.view().copy_from(ptr->_origin_mesh->mesh_data()->pack.data);
            if (init_last_vertex)
                args.cmdlist << ptr->_last_vertex.view().copy_from(pos_buffer);
        }
    });
}
}// namespace rbc