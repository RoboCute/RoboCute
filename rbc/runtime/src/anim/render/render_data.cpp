#include "rbc_anim/render/render_data.h"

namespace rbc {

void SkeletalMeshRenderData::InitializeWithRefSkeleton(const rbc::ReferenceSkeleton &InRefSkeleton) {

    required_bones.resize_uninitialized(InRefSkeleton.NumJoints());
    for (auto i = 0; i < InRefSkeleton.NumJoints(); ++i) {
        required_bones[i] = (BoneIndexType)i;
    }
    LUISA_INFO("Initialize SkelMeshRenderData with {} Bones", required_bones.size());
}

void SkelMeshRenderDataLOD::Initialize(const world::MeshResource *InMeshResource) {
    // 因为只有vertex buffer和动态相关，所以只有一个buffer
    // buffers.resize_uninitialized(1);// CPU Buffers
    // int prims_count = InMeshResource->primitives.size();
    // skin_primitives.reserve(prims_count);
    // // allocate dynamic mesh primitives
    // size_t buffer_size = 0, position_offset = 0, normal_offset = 0, tangent_offset = 0;
    // const auto *mesh = InMeshResource;
    // for (auto i = 0; i < prims_count; ++i) {
    //     const auto &prim = InMeshResource->primitives[i];
    //     auto vertex_count = prim.vertex_count;
    //     auto &sk_prim = skin_primitives.emplace().ref();
    //     sk_prim.vertex_count = vertex_count;

    //     for (auto &view : prim.vertex_buffers) {
    //         if (view.attribute == EVertexAttribute::JOINTS) {
    //             sk_prim.ref_joints = &view;
    //         } else if (view.attribute == EVertexAttribute::WEIGHTS) {
    //             sk_prim.ref_weights = &view;
    //         } else if (view.attribute == EVertexAttribute::POSITION) {
    //             sk_prim.ref_position = &view;
    //         } else if (view.attribute == EVertexAttribute::NORMAL) {
    //             sk_prim.b_use_normals = true;
    //             sk_prim.ref_normals = &view;
    //         }

    //         else if (view.attribute == EVertexAttribute::TANGENT) {
    //             sk_prim.ref_tangents = &view;
    //             sk_prim.b_use_tangents = true;
    //         }
    //     }
    //     // Mesh vertices should be valid (position/joints/weights exists)
    //     SKR_ASSERT(sk_prim.ref_position && sk_prim.ref_joints && sk_prim.ref_weights);
    //     // calculate offset in dynamic data buffer
    //     position_offset = buffer_size;
    //     buffer_size += vertex_count * sk_prim.ref_position->stride;
    //     {
    //         // allocate dynamic position buffer, copy layout
    //         sk_prim.position.buffer_index = 0;
    //         sk_prim.position.attribute = sk_prim.ref_position->attribute;
    //         sk_prim.position.attribute_index = sk_prim.ref_position->attribute_index;
    //         sk_prim.position.offset = static_cast<uint32_t>(position_offset);
    //         sk_prim.position.stride = sk_prim.ref_position->stride;
    //     }
    //     normal_offset = buffer_size;
    //     if (sk_prim.b_use_normals) {
    //         // use normals, allocate dynamic normal buffer
    //         buffer_size += vertex_count * sk_prim.ref_normals->stride;
    //         sk_prim.normals.buffer_index = 0;
    //         sk_prim.normals.attribute = sk_prim.ref_normals->attribute;
    //         sk_prim.normals.attribute_index = sk_prim.ref_normals->attribute_index;
    //         sk_prim.normals.offset = static_cast<uint32_t>(normal_offset);
    //         sk_prim.normals.stride = sk_prim.ref_normals->stride;
    //     }
    //     tangent_offset = buffer_size;
    //     if (sk_prim.b_use_tangents) {
    //         // use normals, allocate dynamic tangents buffer
    //         buffer_size += vertex_count * sk_prim.ref_tangents->stride;
    //         sk_prim.tangents.buffer_index = 0;
    //         sk_prim.tangents.attribute = sk_prim.ref_tangents->attribute;
    //         sk_prim.tangents.attribute_index = sk_prim.ref_tangents->attribute_index;
    //         sk_prim.tangents.offset = static_cast<uint32_t>(tangent_offset);
    //         sk_prim.tangents.stride = sk_prim.ref_tangents->stride;
    //     }
    // }
    // skin_vertex_buffer_size = buffer_size;
    // auto blob = skr::IBlob::CreateAligned(nullptr, buffer_size, 16, false);// the cpu swap buffer for skinning
    // buffers[0] = blob.get();
}

void SkelMeshRenderDataLOD::InitResources(SkeletalMeshRenderData *InRenderData) {
    // 动态，应该跟随RenderObject初始化
    LUISA_INFO("Initializing Resources for SKelMehsRenderData");
    const auto *InMeshResource = InRenderData->static_mesh_;
    render_data_ = InRenderData;
    Initialize(InMeshResource);

    // for (auto j = 0u; j < buffers.size(); j++) {
    //     if (!vertex_buffers[j]) {
    //         SkrZoneScopedN("AllocateSkeletalMeshGPUBuffer");
    //         auto vb_desc = make_zeroed<CGPUBufferDescriptor>();
    //         vb_desc.name = (const char8_t *)InMeshResource->name.c_str();
    //         vb_desc.usages = CGPU_BUFFER_USAGE_SHADER_READWRITE || CGPU_BUFFER_USAGE_VERTEX_BUFFER;
    //         vb_desc.memory_usage = CGPU_MEM_USAGE_GPU_ONLY;
    //         vb_desc.size = buffers[j]->get_size();
    //         SKR_ASSERT(vb_desc.size > 0);

    //         vertex_buffers[j] = cgpu_create_buffer(InDevice, &vb_desc);

    //         auto renderMesh = InMeshResource->render_mesh;

    //         // dynamic vertex buffer views
    //         vertex_buffer_views.reserve(renderMesh->vertex_buffer_views.size());

    //         for (size_t k = 0; k < skin_primitives.size(); ++k) {
    //             auto &prim = skin_primitives[k];
    //             auto vbv_start = vertex_buffer_views.size();

    //             for (size_t z = 0; z < renderMesh->primitive_commands[k].vbvs.size(); ++z) {
    //                 auto &vbv = renderMesh->primitive_commands[k].vbvs[z];
    //                 auto attr = InMeshResource->primitives[k].vertex_buffers[z].attribute;

    //                 if (attr == EVertexAttribute::POSITION) {
    //                     auto &view = vertex_buffer_views.add_default().ref();
    //                     view.buffer = vertex_buffers[j];
    //                     view.offset = prim.position.offset;
    //                     view.stride = prim.position.stride;
    //                 } else if (attr == EVertexAttribute::NORMAL) {
    //                     auto &view = vertex_buffer_views.add_default().ref();
    //                     view.buffer = vertex_buffers[j];
    //                     view.offset = prim.normals.offset;
    //                     view.stride = prim.normals.stride;
    //                 } else if (attr == EVertexAttribute::TANGENT) {
    //                     auto &view = vertex_buffer_views.add_default().ref();
    //                     view.buffer = vertex_buffers[j];
    //                     view.offset = prim.tangents.offset;
    //                     view.stride = prim.tangents.stride;
    //                 } else {
    //                     vertex_buffer_views.add(vbv);// use original vbv
    //                 }
    //             }
    //             prim.vertex_buffer_views = skr::Span<skr::VertexBufferView>(
    //                 vertex_buffer_views.data() + vbv_start,
    //                 renderMesh->primitive_commands[k].vbvs.size());
    //         }
    //     }
    //     const auto vertex_size = buffers[j]->get_size();
    // }
}

void SkelMeshRenderDataLOD::ReleaseResources() {
    // for (auto vb : vertex_buffers) {
    //     if (vb) cgpu_free_buffer(vb);
    // }

    // buffers.clear();
}

}// namespace rbc
