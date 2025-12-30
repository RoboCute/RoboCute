#include <rbc_graphics/skinning.h>
#include <rbc_graphics/shader_manager.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/byte_buffer.h>
namespace rbc {
void Skinning::load_shader(luisa::fiber::counter &init_counter) {
    ShaderManager::instance()->async_load(init_counter, "geometry/skinning.bin", _skinning);
}
static Skinning *_skining_ptr{};
Skinning * Skinning::instance() {
    return _skining_ptr;
}
Skinning::Skinning() {
    LUISA_ASSERT(!_skining_ptr);
    _skining_ptr = this;
}
Skinning::~Skinning() {
    LUISA_ASSERT(_skining_ptr == this);
    _skining_ptr = nullptr;
}

void Skinning::update_mesh(
    CommandList &cmdlist,
    MeshManager::MeshData *out_mesh,
    MeshManager::MeshData *in_mesh,
    BufferView<DualQuaternion> dual_quat_skeleton,
    BufferView<float> skin_weights,
    BufferView<uint> skin_indices) {
    uint vert_count = out_mesh->meta.vertex_count;
    /// Assert
    LUISA_ASSERT(
        vert_count == in_mesh->meta.vertex_count &&
            ((out_mesh->meta.ele_mask & MeshManager::MeshMeta::normal_mask) == (in_mesh->meta.ele_mask & MeshManager::MeshMeta::normal_mask)),
        "Skinning mesh must have same vertex count and layout than origin mesh.");
    LUISA_ASSERT(skin_weights.size() == skin_indices.size() && skin_weights.size() % vert_count == 0, "Illegal skin weights and indices size.");
    /// Assert

    uint per_vert_weight = skin_weights.size() / vert_count;
    cmdlist << (*_skinning)(
                   ByteBufferView{in_mesh->pack.data},
                   ByteBufferView{out_mesh->pack.mutable_data},
                   dual_quat_skeleton,
                   skin_indices,
                   skin_weights,
                   per_vert_weight,
                   vert_count,
                   (out_mesh->meta.ele_mask & MeshManager::MeshMeta::normal_mask) != 0,
                   (out_mesh->meta.ele_mask & MeshManager::MeshMeta::tangent_mask) != 0)
                   .dispatch(vert_count);
}

}// namespace rbc