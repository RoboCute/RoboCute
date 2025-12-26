#include "rbc_anim/render/skelmesh_render_cpu_skin.h"
#include "rbc_anim/types.h"
#include "ozz/base/span.h"

namespace rbc {

void DoCPUSkinPrimitive(const SkinPrimitive &sk_prim, const SkelMeshRenderDataLODCPU *InRenderData, luisa::span<AnimFloat4x4> InReferenceToLocal) {
    auto *mesh = InRenderData->render_data_->static_mesh_;

    int vertex_count = sk_prim.vertex_count;
    // Fetch span from static mesh data
    auto static_buffer_span = [&vertex_count, &mesh](const VertexBufferEntry *buffer, auto t, uint32_t comps = 1) {
        using T = typename decltype(t)::type;
        SKR_ASSERT(buffer->stride == sizeof(T) * comps);
        auto offset = mesh->bins[buffer->buffer_index].blob->get_data() + buffer->offset;
        return ozz::span<const T>{(T *)offset, vertex_count * comps};
    };

    // Fetch span from dynamic data
    auto dynamic_buffer_span = [&vertex_count, &InRenderData](const VertexBufferEntry *buffer, auto t, uint32_t comps = 1) {
        using T = typename decltype(t)::type;
        SKR_ASSERT(buffer->stride == sizeof(T) * comps);
        auto bin = InRenderData->buffers[buffer->buffer_index];
        auto offset = InRenderData->buffers[buffer->buffer_index]->get_data() + buffer->offset;
        return ozz::span<T>{(T *)offset, vertex_count * comps};
    };

    AnimSkinningJob job;
    // BASIC CONFIG

    job.vertex_count = sk_prim.vertex_count;
    job.joint_matrices = {InReferenceToLocal.data(), InReferenceToLocal.size()};
    job.vertex_count = vertex_count;
    job.influences_count = 4;

    // INPUT LAYOUT
    job.joint_indices = static_buffer_span(sk_prim.ref_joints, skr::type_t<uint16_t>(), 4);
    job.joint_indices_stride = sk_prim.ref_joints->stride;
    // SKR_LOG_FMT_INFO(u8"Joint Indicies: {} with vertex count {}", job.joint_indices.size(), vertex_count);
    // for (auto i = 0; i < 10; i++)
    // {
    //     SKR_LOG_FMT_INFO(u8"Joint Index {}, {}, {}, {}", job.joint_indices[4 * i], job.joint_indices[4 * i + 1], job.joint_indices[4 * i + 2], job.joint_indices[4 * i + 3]);
    // }
    job.joint_weights = static_buffer_span(sk_prim.ref_weights, skr::type_t<float>(), 4);
    job.joint_weights_stride = sk_prim.ref_weights->stride;
    // for (auto i = 0; i < 10; i++)
    // {
    //     SKR_LOG_FMT_INFO(u8"Joint Weight {}: {}", i, job.joint_weights[i]);
    // }
    if (sk_prim.UseNormals()) {
        job.in_normals = static_buffer_span(sk_prim.ref_normals, skr::type_t<float>(), 3);
        job.in_normals_stride = sk_prim.ref_normals->stride;
    }

    if (sk_prim.UseTangents()) {
        job.in_tangents = static_buffer_span(sk_prim.ref_tangents, skr::type_t<float>(), 4);
        job.in_tangents_stride = sk_prim.ref_tangents->stride;
    }

    job.in_positions = static_buffer_span(sk_prim.ref_position, skr::type_t<float>(), 3);
    job.in_positions_stride = sk_prim.ref_position->stride;

    // OUTPUT LAYOUT

    job.out_positions = dynamic_buffer_span(&sk_prim.position, skr::type_t<float>(), 3);
    job.out_positions_stride = sk_prim.position.stride;

    if (sk_prim.UseNormals()) {
        job.out_normals = dynamic_buffer_span(&sk_prim.normals, skr::type_t<float>(), 3);
        job.out_normals_stride = sk_prim.normals.stride;
    }

    if (sk_prim.UseTangents()) {
        job.out_tangents = dynamic_buffer_span(&sk_prim.tangents, skr::type_t<float>(), 4);
        job.out_tangents_stride = sk_prim.tangents.stride;
    }

    if (!job.Run()) {
        SKR_LOG_ERROR(u8"CPUSkinJob Failed");
    }
}

void DoCPUSkin(const SkelMeshRenderDataLODCPU *InRenderData, luisa::span<AnimFloat4x4> InReferenceToLocal) {
    for (auto i = 0; i < InRenderData->skin_primitives.size(); i++) {
        // create job per primitive
        DoCPUSkinPrimitive(InRenderData->skin_primitives[i], InRenderData, InReferenceToLocal);
    }
}

}// namespace rbc

namespace rbc {

SkeletalMeshRenderObjectCPUSkin::SkeletalMeshRenderObjectCPUSkin(const SkeletalMesh *InSkelMesh, skr::RenderDevice *InRenderDevice)
    : SkeletalMeshRenderObjectCPUSkin(SkeletalMeshSceneProxyDesc(InSkelMesh), InRenderDevice) {
}

SkeletalMeshRenderObjectCPUSkin::SkeletalMeshRenderObjectCPUSkin(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc, skr::RenderDevice *InRenderDevice)
    : SkeletalMeshRenderObject(InSkelMeshDesc, InRenderDevice), cached_vertex_lod_(INVALID_INDEX) {
    InitResources(InSkelMeshDesc);
}

void SkeletalMeshRenderObjectCPUSkin::InitResources(const SkeletalMeshSceneProxyDesc &InSkelMeshDesc) {
    LOD.InitResources(InSkelMeshDesc.render_data, render_device_->get_cgpu_device());
}

void SkeletalMeshRenderObjectCPUSkin::ReleaseResources() {
    LOD.ReleaseResources();
}

SkeletalMeshRenderObjectCPUSkin::~SkeletalMeshRenderObjectCPUSkin() {
}

/**
 * Update
 * @param rg: the input render graph
 * @param LODIndex: 未来传入LODIndex的placeholder，从RenderData中找到对应LODIndex的渲染数据，暂时没有作用
 * @param InDynamicData: 动画采样完毕之后的ComponentSpaceBoneTransform入口
 * @param InRenderData: SkeletalMesh对应的渲染数据入口
 * 整个更新流程主要完成：
 * * 将ComponentSpaceBoneTransform批量乘InverseBindMatrix，得到蒙皮需要的ReferenceToLocal矩阵
 * * 将ReferenceToLocal矩阵和对应LOD的渲染数据进行CPU蒙皮，得到更新后的VertexBuffer
 * * 将更新后的VertexBuffer缓存并上传到GPU，方便渲染器调用
 */
void SkeletalMeshRenderObjectCPUSkin::Update(AnimRenderState &state, int32_t LODIndex, const SkeletalMeshSceneProxyDynamicData &InDynamicData, const SkinResource *InRefSkin) {
    UpdateRefToLocalMatrices(LOD.skin_matrices, InDynamicData, InRefSkin);
    // 当前直接同步计算，未来会推入执行队列中异步计算
    UpdateDynamicData_RenderThread(state.rg);
}

void SkeletalMeshRenderObjectCPUSkin::UpdateDynamicData_RenderThread(skr::RG::RenderGraph *rg) {
    CPUSkinAndCacheVertices(rg);
    // override primitive commands
    if (true) {
        UpdateDrawcalls();
    }
    // override BLAS
    if (true) {
        UpdateGPUScene();
    }
}

void SkeletalMeshRenderObjectCPUSkin::CPUSkinAndCacheVertices(skr::RG::RenderGraph *rg) {
    SkrZoneScopedN("DoCPUSkinAndUpload");
    DoCPUSkin(&LOD, LOD.skin_matrices);
    int buffer_size = LOD.skin_vertex_buffer_size;

    // 每一帧都create buffer handle，RenderGraph内部走pooled
    auto upload_buffer_handle = rg->create_buffer(
        [=](skr::RG::RenderGraph &rg, skr::RG::BufferBuilder &builder) {
            builder.set_name(u8"SkinDynamicDataUploadBuffer")
                .size(buffer_size)
                .with_tags(kRenderGraphDefaultResourceTag)
                .as_upload_buffer();
        });

    rg->add_copy_pass(
        [=](skr::RG::RenderGraph &rg, skr::RG::CopyPassBuilder &builder) {
            builder.set_name(u8"UploadSkinMesh")
                .from_buffer(upload_buffer_handle.range(0, buffer_size))
                .can_be_lone();
        },
        [=, this](skr::RG::RenderGraph &rg, skr::RG::CopyPassContext &context) {
            SkrZoneScopedN("UploadSkinMesh");

            auto upload_buffer = context.resolve(upload_buffer_handle);
            auto mapped = (uint8_t *)upload_buffer->info->cpu_mapped_address;

            // barrier from vb to copy dest
            {
                CGPUResourceBarrierDescriptor barrier_desc = {};
                skr::Vector<CGPUBufferBarrier> barriers;

                for (size_t j = 0u; j < LOD.buffers.size(); j++) {
                    if (LOD.vertex_buffers[j]) {
                        CGPUBufferBarrier &barrier = barriers.emplace().ref();
                        barrier.buffer = LOD.vertex_buffers[j];
                        barrier.src_state = CGPU_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
                        barrier.dst_state = CGPU_RESOURCE_STATE_COPY_DEST;
                    }
                }

                barrier_desc.buffer_barriers = barriers.data();
                barrier_desc.buffer_barriers_count = (uint32_t)barriers.size();
                cgpu_cmd_resource_barrier(context.cmd, &barrier_desc);
            }

            // upload
            {
                SkrZoneScopedN("MemCopies");
                uint64_t cursor = 0;
                const bool use_dynamic_buffer = LOD.render_data_->use_dynamic_buffer;
                if (!use_dynamic_buffer) {
                    for (size_t j = 0u; j < LOD.buffers.size(); j++) {
                        // copy render_data CPU Buffer to mapped upload buffer
                        memcpy(
                            mapped + cursor,
                            LOD.buffers[j]->get_data(),
                            LOD.buffers[j]->get_size());

                        // Copy upload buffer to RenderData Dynamic GPU Buffer
                        CGPUBufferToBufferTransfer b2b = {};
                        b2b.src = upload_buffer;
                        b2b.src_offset = cursor;
                        b2b.dst = LOD.vertex_buffers[j];
                        b2b.dst_offset = cursor;
                        b2b.size = LOD.buffers[j]->get_size();
                        cgpu_cmd_transfer_buffer_to_buffer(context.cmd, &b2b);
                        cursor += LOD.buffers[j]->get_size();
                    }
                }
            }
        });
}

void SkeletalMeshRenderObjectCPUSkin::UpdateDrawcalls() {
    auto *mesh_resource = LOD.render_data_->static_mesh_;
    auto *render_mesh = mesh_resource->render_mesh;
    for (auto i = 0; i < render_mesh->primitive_commands.size(); i++) {
        render_mesh->primitive_commands[i].vbvs = LOD.skin_primitives[i].vertex_buffer_views;
    }
}

void SkeletalMeshRenderObjectCPUSkin::UpdateGPUScene() {
    auto *mesh_resource = LOD.render_data_->static_mesh_;
    auto *render_mesh = mesh_resource->render_mesh;
    skr::InlineVector<CGPUAccelerationStructureGeometryDesc, 4> geoms;

    for (const auto &section : mesh_resource->sections) {
        const auto section_transform = skr::TransformF(
            skr::QuatF(section.rotation), section.translation, section.scale);
        const auto transform = section_transform.to_matrix();
        float transform34[12] = {
            transform.m00, transform.m10, transform.m20, transform.m30,// Row 0: X axis + X translation
            transform.m01, transform.m11, transform.m21, transform.m31,// Row 1: Y axis + Y translation
            transform.m02, transform.m12, transform.m22, transform.m32 // Row 2: Z axis + Z translation
        };

        for (const auto &prim_id : section.primitive_indices) {
            const auto &primitive = mesh_resource->primitives[prim_id];

            CGPUAccelerationStructureGeometryDesc geom = {};
            geom.flags = CGPU_ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NONE;      // section.is_raytracing_opaque ? CGPU_ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE : CGPU_ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NONE;
            geom.vertex_buffer = LOD.vertex_buffers[0];                       // override
            geom.vertex_offset = LOD.skin_primitives[prim_id].position.offset;// override
            geom.vertex_count = primitive.vertex_count;
            geom.vertex_stride = LOD.skin_primitives[prim_id].position.stride;// override
            geom.vertex_format = CGPU_FORMAT_R32G32B32_SFLOAT;
            geom.index_buffer = render_mesh->buffers[primitive.index_buffer.buffer_index];
            geom.index_offset = primitive.index_buffer.index_offset;
            geom.index_count = primitive.index_buffer.index_count;
            // D3D12 expects index_stride to be 2 (uint16) or 4 (uint32)
            // primitive.index_buffer.stride should already be 2 or 4, but let's ensure it
            geom.index_stride = (primitive.index_buffer.stride == 2) ? sizeof(uint16_t) : sizeof(uint32_t);
            memcpy(geom.transform, transform34, sizeof(transform34));
            geoms.add(geom);
        }
    }
    CGPUAccelerationStructureDescriptor blas_desc = {};
    blas_desc.type = CGPU_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blas_desc.bottom.geometries = geoms.data();
    blas_desc.bottom.count = geoms.size();
    // recreate blas
    // TODO: refit BLAS?
    if (render_mesh->blas) {
        cgpu_free_acceleration_structure(render_mesh->blas);
    }

    render_mesh->blas = cgpu_create_acceleration_structure(geoms[0].vertex_buffer->device, &blas_desc);
    render_mesh->need_build_blas = true;
}

}// namespace rbc