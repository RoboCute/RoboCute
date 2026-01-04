#include <rbc_graphics/accel_manager.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/raster/raster_scene.h>
#include <rbc_graphics/shader_manager.h>
namespace rbc {

namespace accel_detail {
static const AccelOption tlas_option{
    // "For TLAS, consider the PREFER_FAST_TRACE flag and perform only rebuilds"
    // https://developer.nvidia.com/blog/best-practices-for-using-nvidia-rtx-ray-tracing-updated/
    .hint = AccelOption::UsageHint::FAST_TRACE,
    .allow_compaction = false,
    .allow_update = false};
}// namespace accel_detail

AccelManager::AccelManager(Device &device)
    : _device(device) {
    VertexAttribute pos_attr{
        VertexAttributeType::Position,
        PixelFormat::RGBA32F};

    _basic_foramt.emplace_vertex_stream({&pos_attr, 1});

    // Initialize buffers with minimum size to avoid null buffer access when scene is empty
    constexpr auto aligned_size_inst = (65536u + sizeof(InstanceInfo) - 1) / sizeof(InstanceInfo);
    constexpr auto aligned_size_trans = (65536u + sizeof(float4x4) - 1) / sizeof(float4x4);
    _inst_buffer = _device.create_buffer<InstanceInfo>(aligned_size_inst);
    _last_trans_buffer = _device.create_buffer<float4x4>(aligned_size_trans);
}
void AccelManager::load_shader(luisa::fiber::counter &counter) {
    counter.add();
    luisa::fiber::schedule([this, counter]() {
        ShaderManager::instance()->load("geometry/update_last_transform.bin", _update_last_transform);
        ShaderManager::instance()->load(
            "geometry/move_the_world.bin",
            _move_the_world);
        ShaderManager::instance()->load(
            "geometry/move_the_world_onlylast.bin",
            _move_the_world_onlylast);
        counter.done();
    });
}
void AccelManager::set_mesh_instance(
    CommandList &cmdlist,
    BufferUploader &uploader,
    uint inst_id,
    float4x4 transform,
    uint8_t visibility_mask,
    bool opaque) {
    auto &inst = _insts[inst_id];
    auto &mesh_data = _accel_elements[inst.accel_id].mesh_data;
    _dirty = true;
    auto &ele = _accel_elements[inst.accel_id];
    ele.transform = transform;
    ele.visibility_mask = visibility_mask;
    ele.opaque = opaque;
    if (!mesh_data.is_type_of<MeshManager::MeshData *>()) [[unlikely]] {
        LUISA_ERROR("Accel instance type mismatch.");
    }
    if (_accel) {
        auto t = mesh_data.force_get<MeshManager::MeshData *>();
        if (!t->pack.mesh) {
            t->build_mesh(_device, cmdlist, AccelOption{.allow_compaction = false});
        }
        _accel.set(
            inst.accel_id,
            t->pack.mesh,
            transform,
            visibility_mask,
            opaque,
            inst_id);
    }
}
void AccelManager::_init_default_procedural_blas(
    CommandList &cmdlist,
    DisposeQueue &disp_queue) {
    if (!_default_aabb_buffer) {
        _default_aabb_buffer = _device.create_buffer<AABB>(1);
        luisa::unique_ptr<AABB> ptr(luisa::new_with_allocator<AABB>());
        ptr->packed_min = {-0.5f, -0.5f, -0.5f};
        ptr->packed_max = {0.5f, 0.5f, 0.5f};
        cmdlist << _default_aabb_buffer.view().copy_from(ptr.get());
        disp_queue.dispose_after_queue(std::move(ptr));
        _default_procedural_prim = _device.create_procedural_primitive(_default_aabb_buffer, AccelOption{});
        cmdlist << _default_procedural_prim.build();
    }
}

void AccelManager::dispose_accel(CommandList &cmdlist, DisposeQueue &disp_queue) {
    if (!_accel) return;
    update_last_transform(cmdlist);
    disp_queue.dispose_after_queue(std::move(_accel));
    _dirty = true;
    for (auto &i : _accel_elements) {
        i.mesh_data.visit([&]<typename T>(T &t) {
            if constexpr (std::is_same_v<T, MeshManager::MeshData *>) {
                disp_queue.dispose_after_queue(std::move(t->pack.mesh));
            } else {
                // Procedural
            }
        });
    }
}

void AccelManager::init_accel(CommandList &cmdlist) {
    if (_accel_elements.empty() || _accel) return;
    _dirty = true;
    _accel = _device.create_accel(accel_detail::tlas_option);
    for (auto &i : _accel_elements) {
        i.mesh_data.visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, MeshManager::MeshData *>) {
                if (!t->pack.mesh) {
                    t->build_mesh(_device, cmdlist, AccelOption{.allow_compaction = false});
                }
                _accel.emplace_back(
                    t->pack.mesh,
                    i.transform,
                    i.visibility_mask,
                    i.opaque,
                    i.user_id);
            } else {
                _accel.emplace_back(
                    t ? t : _default_procedural_prim,
                    i.transform,
                    i.visibility_mask,
                    i.user_id);
            }
        });
    }
}

uint AccelManager::emplace_procedural_instance(
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    ProceduralPrimitive &&prim,
    ProceduralVariant &&prim_data,
    float4x4 const &transform,
    uint8_t visibility_mask) {
    if (!_procedural_type_buffer) {
        _procedural_type_buffer = _device.create_buffer<ProceduralType>((65536u + sizeof(ProceduralType) - 1) / sizeof(ProceduralType));
    }
    if (!prim) {
        _init_default_procedural_blas(cmdlist, disp_queue);
    }
    uint inst_id;
    if (_procedural_user_id_pool.empty()) {
        inst_id = _procedural_insts.size();
        _procedural_insts.emplace_back();
        auto resize_buffer = [&]<typename T>(Buffer<T> &buffer) {
            if (_procedural_insts.size() <= buffer.size()) [[likely]]
                return;
            auto aligned_size = 65536 / sizeof(T);
            auto size = (buffer.size() * 1.5 + aligned_size - 1) / aligned_size * aligned_size;
            auto new_buffer = _device.create_buffer<T>(size);
            uploader.swap_buffer(buffer, new_buffer);
            cmdlist << new_buffer.view(0, buffer.size()).copy_from(buffer);
            disp_queue.dispose_after_queue(std::move(buffer));
            buffer = std::move(new_buffer);
        };
        resize_buffer(_procedural_type_buffer);
    } else {
        inst_id = _procedural_user_id_pool.back();
        _procedural_user_id_pool.pop_back();
    }
    auto &inst = _procedural_insts[inst_id];
    _dirty = true;
    inst.accel_id = _accel_elements.size();
    _update_procedural_instance(inst_id, cmdlist, temp_buffer, buffer_allocator, uploader, disp_queue, std::move(prim_data));
    if (_accel)
        _accel.emplace_back(
            prim ? prim : _default_procedural_prim,
            transform,
            visibility_mask,
            inst_id);
    _accel_elements.emplace_back(
        transform,
        visibility_mask,
        std::move(prim),
        inst_id);
    return inst_id;
}
void AccelManager::set_procedural_instance(
    uint inst_id,
    float4x4 transform,
    uint8_t visibility_mask,
    bool opaque) {
    auto &inst = _procedural_insts[inst_id];
    auto &mesh_data = _accel_elements[inst.accel_id].mesh_data;
    _dirty = true;
    auto &ele = _accel_elements[inst.accel_id];
    ele.transform = transform;
    ele.visibility_mask = visibility_mask;
    ele.opaque = opaque;
    if (!mesh_data.is_type_of<ProceduralPrimitive>()) [[unlikely]] {
        LUISA_ERROR("Accel instance type mismatch.");
    }
    if (_accel)
        _accel.set(
            inst.accel_id,
            mesh_data.force_get<ProceduralPrimitive>(),
            transform,
            visibility_mask,
            inst_id);
}
void AccelManager::_update_procedural_instance(
    uint inst_id,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    ProceduralVariant &&prim_data) {
    auto &inst = _procedural_insts[inst_id];
    ProceduralType type{
        .type = (uint)prim_data.index(),
        .meta_byte_offset = ~0u};
    auto before_copy = [&](Buffer<uint> const &old_buffer, Buffer<uint> const &new_buffer) {
        uploader.swap_buffer(old_buffer, new_buffer);
    };
    prim_data.visit([&]<typename T>(T const &t) {
        if (!inst.meta_node)
            inst.meta_node = buffer_allocator.allocate(
                _device,
                cmdlist,
                disp_queue,
                sizeof(T),
                before_copy);
        auto bf_view = inst.meta_node.view<T>(buffer_allocator);
        uploader.emplace_copy_cmd(bf_view, &t);
        type.meta_byte_offset = bf_view.offset_bytes();
    });

    uploader.emplace_copy_cmd(_procedural_type_buffer.view(inst_id, 1), &type);
}

void AccelManager::_update_mesh_instance(
    uint inst_id,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    MeshManager::MeshData *mesh_data,
    span<const MatCode> mat_codes,
    float4x4 const &transform,
    uint8_t visibility_mask,
    bool opaque,
    uint light_id,
    uint8_t light_mask,
    bool reset_last,
    uint last_vert_buffer_id,
    uint vis_buffer_heap_idx) {
    LUISA_ASSERT(light_id <= 0xffffffu, "Light id out of range.");
    LUISA_ASSERT(!(vis_buffer_heap_idx != -1 && opaque), "Opaque object can-not have vis_buffer.");
    auto &inst = _insts[inst_id];
    uint mat_index;
    if (!mesh_data->submesh_offset.empty()) {
        LUISA_ASSERT(mesh_data->submesh_offset.size() == mat_codes.size(), "Submesh count and material count mismatch.");

        // in case buffer resize, clean all copy-command
        auto before_copy = [&](Buffer<uint> const &old_buffer, Buffer<uint> const &new_buffer) {
            uploader.swap_buffer(old_buffer, new_buffer);
        };
        if (!inst.material_node)
            inst.material_node = buffer_allocator.allocate(
                _device,
                cmdlist,
                disp_queue,
                mat_codes.size_bytes(),
                before_copy);
        mat_index = inst.material_node.offset_bytes() / sizeof(uint);
        auto bf_view = inst.material_node.view<uint>(buffer_allocator);
        if (mat_codes.size() < 16) {
            uploader.emplace_copy_cmd(bf_view, reinterpret_cast<uint const *>(mat_codes.data()));
        } else {
            auto host_upload_buffer = temp_buffer.allocate_upload_buffer<uint>(mat_codes.size());
            memcpy(host_upload_buffer.mapped_ptr(), mat_codes.data(), mat_codes.size_bytes());
            cmdlist << bf_view.subview(0, mat_codes.size()).copy_from(host_upload_buffer.view);
        }
    } else {
        LUISA_ASSERT(mat_codes.size() == 1, "Material count must be one.");
        mat_index = mat_codes[0].value;
    }
    InstanceInfo inst_info{
        .mesh = mesh_data->meta,
        .mat_index = mat_index,
        // TODO: custom properties
        .light_mask_id = light_id | (static_cast<uint>(light_mask) << 24u),
        .last_vertex_heap_idx = last_vert_buffer_id// TODO: vertex animation
    };
    uploader.emplace_copy_cmd(_inst_buffer.view(inst_id, 1), &inst_info);
    if (reset_last || (!_accel) /* never consider motion-vector in non-raytracing mode */) {
        uploader.emplace_copy_cmd(_last_trans_buffer.view(inst_id, 1), &transform);
    }
    if (!opaque) {
        *uploader.emplace_copy_cmd(_triangle_vis_buffer.view(inst_id, 1)) = vis_buffer_heap_idx;
    }
}
uint AccelManager::emplace_mesh_instance(
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    MeshManager::MeshData *mesh_data,
    span<const MatCode> mat_codes,
    float4x4 const &transform,
    uint8_t visibility_mask,
    bool opaque,
    uint light_id,
    uint8_t light_mask,
    uint last_vert_buffer_id,
    uint vis_buffer_heap_idx) {
    if (!_inst_buffer)
        _inst_buffer = _device.create_buffer<InstanceInfo>((65536u + sizeof(InstanceInfo) - 1) / sizeof(InstanceInfo));
    if (!_last_trans_buffer)
        _last_trans_buffer = _device.create_buffer<float4x4>((65536u + sizeof(float4x4) - 1) / sizeof(float4x4));
    if (!_triangle_vis_buffer)
        _triangle_vis_buffer = _device.create_buffer<uint>((65536u + sizeof(uint) - 1) / sizeof(uint));
    uint inst_id;
    if (_user_id_pool.empty()) {
        inst_id = _insts.size();
        _insts.emplace_back();
        // resize
        auto resize_buffer = [&]<typename T>(Buffer<T> &buffer) {
            if (_insts.size() <= buffer.size()) [[likely]]
                return;
            auto aligned_size = 65536 / sizeof(T);
            auto size = (buffer.size() * 1.5 + aligned_size - 1) / aligned_size * aligned_size;
            auto new_buffer = _device.create_buffer<T>(size);
            uploader.swap_buffer(buffer, new_buffer);
            cmdlist << new_buffer.view(0, buffer.size()).copy_from(buffer);
            disp_queue.dispose_after_queue(std::move(buffer));
            buffer = std::move(new_buffer);
        };
        resize_buffer(_inst_buffer);
        resize_buffer(_last_trans_buffer);

        if (!opaque) {
            resize_buffer(_triangle_vis_buffer);
        }
    } else {
        inst_id = _user_id_pool.back();
        _user_id_pool.pop_back();
    }
    auto &inst = _insts[inst_id];
    _update_mesh_instance(
        inst_id,
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        mesh_data,
        mat_codes,
        transform,
        visibility_mask,
        opaque,
        light_id,
        light_mask,
        true,
        last_vert_buffer_id,
        vis_buffer_heap_idx);
    _dirty = true;
    inst.accel_id = _accel_elements.size();
    _accel_elements.emplace_back(
        transform,
        visibility_mask,
        opaque,
        mesh_data,
        inst_id);
    if (_accel) {
        if (!mesh_data->pack.mesh) {
            mesh_data->build_mesh(_device, cmdlist, AccelOption{.allow_compaction = false});
        }
        _accel.emplace_back(
            mesh_data->pack.mesh,
            transform,
            visibility_mask,
            opaque,
            inst_id);
    }

    return inst_id;
}
void AccelManager::set_procedural_instance(
    uint inst_id,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    ProceduralPrimitive &&prim,
    ProceduralVariant &&prim_data,
    float4x4 const &transform,
    uint8_t visibility_mask) {
    auto &inst = _procedural_insts[inst_id];
    _update_procedural_instance(inst_id, cmdlist, temp_buffer, buffer_allocator, uploader, disp_queue, std::move(prim_data));

    if (!prim) {
        _init_default_procedural_blas(cmdlist, disp_queue);
    }
    _dirty = true;
    if (_accel)
        _accel.set(
            inst.accel_id,
            prim ? prim : _default_procedural_prim,
            transform,
            visibility_mask,
            inst_id);
    auto &accel_ele = _accel_elements[inst.accel_id];
    auto procedural = accel_ele.mesh_data.try_get<ProceduralPrimitive>();
    if (procedural && *procedural) {
        disp_queue.dispose_after_queue(std::move(*procedural));
    }
    accel_ele = AccelElement(
        transform,
        visibility_mask,
        std::move(prim),
        inst_id);
}
void AccelManager::set_mesh_instance(
    uint inst_id,
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    MeshManager::MeshData *mesh_data,
    span<const MatCode> mat_codes,
    float4x4 const &transform,
    uint8_t visibility_mask,
    bool opaque,
    uint light_id,
    uint8_t light_mask,
    bool reset_last,
    uint last_vert_buffer_id,
    uint vis_buffer_heap_idx) {
    auto &inst = _insts[inst_id];
    _update_mesh_instance(
        inst_id,
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        mesh_data,
        mat_codes,
        transform,
        visibility_mask,
        opaque,
        light_id,
        light_mask,
        reset_last,
        last_vert_buffer_id,
        vis_buffer_heap_idx);
    _dirty = true;
    _accel_elements[inst.accel_id] = AccelElement(
        transform,
        visibility_mask,
        opaque,
        mesh_data,
        inst_id);
    if (_accel) {
        if (!mesh_data->pack.mesh) {
            mesh_data->build_mesh(_device, cmdlist, AccelOption{.allow_compaction = false});
        }
        _accel.set(
            inst.accel_id,
            mesh_data->pack.mesh,
            transform,
            visibility_mask,
            opaque,
            inst_id);
    }
}
void AccelManager::_swap_last(BufferUploader &uploader, auto &inst, DisposeQueue *disp_queue) {
    _dirty = true;
    // swap last element
    auto &accel_ele = _accel_elements[inst.accel_id];
    auto procedural = accel_ele.mesh_data.try_get<ProceduralPrimitive>();
    if (procedural && *procedural) {
        if (!disp_queue) {
            LUISA_ERROR("Remove accel type mismatch.");
        }
        disp_queue->dispose_after_queue(std::move(*procedural));
    }
    if (inst.accel_id != (_accel_elements.size() - 1)) {
        accel_ele = std::move(_accel_elements.back());
        accel_ele.mesh_data.visit([&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, MeshManager::MeshData *>) {
                _insts[accel_ele.user_id].accel_id = inst.accel_id;
                if (_accel) {
                    _accel.set(
                        inst.accel_id,
                        t->pack.mesh,
                        accel_ele.transform,
                        accel_ele.visibility_mask,
                        accel_ele.opaque,
                        accel_ele.user_id);
                }
            } else {
                _procedural_insts[accel_ele.user_id].accel_id = inst.accel_id;
                if (_accel)
                    _accel.set(
                        inst.accel_id,
                        t,
                        accel_ele.transform,
                        accel_ele.visibility_mask,
                        accel_ele.user_id);
            }
        });
    }

    _accel_elements.pop_back();
    if (_accel)
        _accel.pop_back();
    inst.accel_id = ~0u - 1u;
}
void AccelManager::remove_mesh_instance(
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    uint inst_idx) {
    auto &inst = _insts[inst_idx];
    LUISA_ASSERT(inst.accel_id != ~0u - 1, "Invalid instance.");
    if (inst.material_node) {
        buffer_allocator.free(inst.material_node);
        inst.material_node = {};
    }
    _user_id_pool.emplace_back(inst_idx);
    _swap_last(uploader, inst, nullptr);
}

void AccelManager::remove_procedural_instance(
    BufferAllocator &buffer_allocator,
    BufferUploader &uploader,
    DisposeQueue &disp_queue,
    uint inst_idx) {
    auto &inst = _procedural_insts[inst_idx];
    LUISA_ASSERT(inst.accel_id != ~0u - 1, "Invalid instance.");
    if (inst.meta_node) {
        buffer_allocator.free(inst.meta_node);
        inst.meta_node = {};
    }
    _procedural_user_id_pool.emplace_back(inst_idx);
    _swap_last(uploader, inst, &disp_queue);
}

auto AccelManager::try_get_accel_element(uint inst_id) const -> AccelElement const * {
    auto id = _insts[inst_id].accel_id;
    if (id >= _accel_elements.size()) return nullptr;
    return &_accel_elements[id];
}

void AccelManager::build_accel(CommandList &cmdlist) {
    if (_dirty && _accel && _accel.size() > 0) {
        cmdlist << _accel.build();
        _dirty = false;
        _accel_builded = true;
    }
}

AccelManager::~AccelManager() {
}
void AccelManager::update_last_transform(
    CommandList &cmdlist) {
    if (!_accel || _accel.size() == 0 || (!_accel_builded)) return;
    cmdlist << (*_update_last_transform)(
                   _accel,
                   _last_trans_buffer)
                   .dispatch(_accel.size());
    _accel_builded = false;
}
void AccelManager::move_the_world(
    CommandList &cmdlist,
    float3 offset) {
    if (_accel_elements.empty()) return;
    if (_accel && _accel.size() > 0) {
        cmdlist << (*_move_the_world)(
                       _accel,
                       _last_trans_buffer,
                       offset)
                       .dispatch(_accel_elements.size());
    } else {
        cmdlist << (*_move_the_world_onlylast)(
                       _last_trans_buffer,
                       offset)
                       .dispatch(_accel_elements.size());
    }
    luisa::fiber::parallel(_accel_elements.size(), [&](size_t i) { _accel_elements[i].transform.cols[3] += make_float4(offset, 0); }, 128);
    _dirty = true;
}

auto AccelManager::draw_object(uint inst_id, uint draw_instance_count, uint object_id, uint submesh_index) -> DrawCommand {
    DrawCommand cmd;
    auto inst = try_get_accel_element(inst_id);
    if (!inst) return cmd;
    LUISA_DEBUG_ASSERT(inst->mesh_data.is_type_of<MeshManager::MeshData *>());
    auto mesh_data = inst->mesh_data.force_get<MeshManager::MeshData *>();
    cmd.info.local_to_world_and_inst_id = inst->transform;
    uint triangle_offset = 0;
    uint triangle_size = mesh_data->triangle_size;
    if (submesh_index != ~0u) {
        LUISA_DEBUG_ASSERT(submesh_index < std::max<uint64_t>(1, mesh_data->submesh_offset.size()));
        if (submesh_index > 0) {
            triangle_offset = mesh_data->submesh_offset[submesh_index];
        }
        triangle_size = (submesh_index + 1) < mesh_data->submesh_offset.size() ?
                            mesh_data->submesh_offset[submesh_index + 1] :
                            mesh_data->triangle_size;

        triangle_size -= triangle_offset;
    }
    reinterpret_cast<uint &>(cmd.info.local_to_world_and_inst_id[2][3]) = triangle_offset;
    reinterpret_cast<uint &>(cmd.info.local_to_world_and_inst_id[3][3]) = inst_id;
    BufferView<uint> vert_buffer = mesh_data->pack.mutable_data ? mesh_data->pack.mutable_data : mesh_data->pack.data;
    BufferView<uint> index_buffer = mesh_data->pack.data ? mesh_data->pack.data : mesh_data->pack.data_view;
    VertexBufferView vbv{
        vert_buffer.subview(0, mesh_data->meta.vertex_count * 4).as<float4>()};
    cmd.mesh = RasterMesh(
        luisa::span{&vbv, 1},
        index_buffer.subview(
            mesh_data->meta.tri_byte_offset / sizeof(uint) + triangle_offset * 3,
            triangle_size * 3),
        draw_instance_count, object_id, 0);
    return cmd;
}

void AccelManager::make_draw_list(
    CommandList &cmdlist,
    DisposeQueue &after_commit_dispqueue,
    DisposeQueue &after_sync_dispqueue,
    vstd::function<bool(float4x4 const &, AABB const &)> const &cull_func,
    DrawListMap &out_draw_meshes,
    BufferView<RasterElement> &out_data_buffer) {
    auto mesh_map = _cache_maps.dequeue();
    if (!mesh_map) {
        mesh_map.create();
    } else {
        mesh_map->clear();
    }
    luisa::spin_mutex map_mtx, elem_mtx;
    std::atomic_uint64_t buffer_size{0};
    luisa::fiber::parallel(
        _accel_elements.size(), [&](uint idx) {
            auto &inst = _accel_elements[idx];
            if (!inst.mesh_data.is_type_of<MeshManager::MeshData *>()) {
                return;
            }
            auto mesh = inst.mesh_data.force_get<MeshManager::MeshData *>();
            map_mtx.lock();
            auto iter = mesh_map->try_emplace(mesh);
            map_mtx.unlock();
            auto &mesh_inst_list = iter.first.value();
            mesh_inst_list.mtx.lock();
            mesh_inst_list.instance_indices.resize(std::max<uint>(mesh->submesh_offset.size(), 1));
            mesh_inst_list.mtx.unlock();
            for (auto submesh_idx : vstd::range(std::max<uint>(mesh->submesh_offset.size(), 1))) {
                // bool keep = false;
                // TODO: cull
                if (!mesh->bbox_requests ||
                    (mesh->bbox_requests->finished &&
                     !cull_func(inst.transform, mesh->bbox_requests->bounding_box[submesh_idx]))) {
                    continue;
                }
                mesh_inst_list.mtx.lock();
                mesh_inst_list.instance_indices[submesh_idx].emplace_back(idx);
                mesh_inst_list.mtx.unlock();
                buffer_size += 1;
            }
        },
        128);
    if (buffer_size == 0) {
        return;
    }
    luisa::vector<RasterElement> elem_host;
    constexpr auto aligned_size = (65536 + sizeof(RasterElement) - 1) / sizeof(RasterElement);
    auto desired_buffer_size = (buffer_size.load() + aligned_size - 1) / aligned_size * aligned_size;
    elem_host.reserve(buffer_size.load());
    out_draw_meshes.reserve(out_draw_meshes.size() + mesh_map->size());
    for (auto &drawcall : *mesh_map) {
        auto &mesh = drawcall.first;
        auto &inst_list = drawcall.second;
        BufferView<uint> vert_buffer_data = mesh->pack.mutable_data ? mesh->pack.mutable_data : mesh->pack.data;
        BufferView<uint> index_buffer_data = mesh->pack.data ? mesh->pack.data : mesh->pack.data_view;
        VertexBufferView vbv{
            vert_buffer_data.subview(0, mesh->meta.vertex_count * 4).as<float4>()};
        auto idx_size = mesh->triangle_size * 3;
        auto index_buffer = index_buffer_data.subview(index_buffer_data.size() - idx_size, idx_size);
        for (auto submesh_idx : vstd::range(std::max<uint>(mesh->submesh_offset.size(), 1))) {
            auto &instance_indices = inst_list.instance_indices[submesh_idx];
            if (instance_indices.empty()) continue;
            auto obj_id = elem_host.size();
            uint buffer_offset = submesh_idx < mesh->submesh_offset.size() ? mesh->submesh_offset[submesh_idx] : 0;
            auto buffer_size = (submesh_idx + 1) < mesh->submesh_offset.size() ? mesh->submesh_offset[submesh_idx + 1] : mesh->triangle_size;
            buffer_size -= buffer_offset;
            BufferView<uint> subview_index_buffer = index_buffer.subview(
                buffer_offset * 3,
                buffer_size * 3);
            out_draw_meshes.emplace_back(
                luisa::span<VertexBufferView const>{&vbv, 1},
                subview_index_buffer,
                instance_indices.size(),
                obj_id);
            for (auto &i : instance_indices) {
                auto &inst = _accel_elements[i];
                auto &elem = elem_host.emplace_back();
                elem.local_to_world_and_inst_id = inst.transform;

                reinterpret_cast<uint &>(elem.local_to_world_and_inst_id[2][3]) = buffer_offset;
                reinterpret_cast<uint &>(elem.local_to_world_and_inst_id[3][3]) = inst.user_id;
            }
        }
    }
    if (_raster_transform_buffer && _raster_transform_buffer.size() < desired_buffer_size) {
        after_sync_dispqueue.dispose_after_queue(std::move(_raster_transform_buffer));
    }
    if (!_raster_transform_buffer) {
        _raster_transform_buffer = _device.create_buffer<RasterElement>(desired_buffer_size);
    }
    out_data_buffer = _raster_transform_buffer.view(0, buffer_size.load());
    cmdlist << out_data_buffer.copy_from(elem_host.data());
    after_commit_dispqueue.dispose_after_queue(std::move(elem_host));
    mesh_map->clear();
    _cache_maps.enqueue(std::move(*mesh_map));
}

void AccelManager::iterate_scene(
    vstd::function<bool(uint user_id, float4x4 const &transform, AABB const &bounding_box)> const &callback// return: true to break submesh
) {
    luisa::fiber::parallel(
        _accel_elements.size(), [&](uint idx) {
            auto &inst = _accel_elements[idx];
            if (!inst.mesh_data.is_type_of<MeshManager::MeshData *>()) {
                return;
            }
            auto mesh = inst.mesh_data.force_get<MeshManager::MeshData *>();
            for (auto submesh_idx : vstd::range(std::max<uint>(mesh->submesh_offset.size(), 1))) {
                // bool keep = false;
                // TODO: cull
                if (!mesh->bbox_requests || !mesh->bbox_requests->finished) {
                    continue;
                }
                if (callback(inst.user_id, inst.transform, mesh->bbox_requests->bounding_box[submesh_idx])) {
                    break;
                }
            }
        },
        128);
}

}// namespace rbc