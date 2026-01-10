#include <rbc_graphics/managed_device.h>
#include <luisa/core/logging.h>
#include <luisa/backends/ext/registry.h>
#include <luisa/backends/ext/raster_cmd.h>

namespace rbc {
#define RBC_NOT_IMPL LUISA_NOT_IMPLEMENTED()
#define RBC_NOT_IMPL_RET     \
    LUISA_NOT_IMPLEMENTED(); \
    return {}
ManagedDevice::ManagedDevice(Context &&ctx, DeviceInterface *impl)
    : DeviceInterface{std::move(ctx)}, _impl(impl), _tex_meta_pool(256), _buffer_meta_pool(256), _tex_handle_pool(256), _buffer_fit(std::numeric_limits<uint64_t>::max(), 16) {
}
ManagedDevice::~ManagedDevice() {
    for (auto &i : _native_resources) {
        _impl->destroy_texture(i->res_info.handle);
    }
    if (_transient_buffer_handle != invalid_resource_handle) {
        _impl->destroy_buffer(_transient_buffer_handle);
    }
}

auto ManagedDevice::_allocate_handle(ManagedTexDesc const &desc) -> TexResourceHandle * {
    auto res_handle = _tex_handle_pool.create();
    res_handle->res_info = _impl->create_texture(
        desc.format, desc.dimension, desc.width, desc.height, desc.depth, desc.mipmap_levels, nullptr, desc.simultaneous_access(), desc.allow_raster_target());
    res_handle->index = _native_resources.size();
    res_handle->last_frame = _frame_index;
    _native_resources.emplace_back(res_handle);
    return res_handle;
}
void ManagedDevice::_deallocate_handle(TexResourceHandle *handle) {
    _tex_dispose_queue.emplace_back(handle->res_info.handle);
    auto &v = _native_resources[handle->index];
    if (handle->index != _native_resources.size() - 1) {
        v = _native_resources.back();
        v->index = handle->index;
    }
    _native_resources.pop_back();
    _tex_handle_pool.destroy(handle);
}

uint64_t ManagedDevice::_get_tex_handle(uint64_t handle) {
    auto iter = _tex_desc_to_native.find(reinterpret_cast<ManagedTexDesc *>(handle));
    if (!iter) {
        return handle;
    }
    auto v = iter.value().handle->res_info.handle;
    return v;
}

std::pair<uint64_t, size_t> ManagedDevice::_get_buffer_handle_offset(uint64_t handle) {
    auto iter = _buffer_desc_to_native.find(reinterpret_cast<size_t *>(handle));
    if (!iter) {
        return {handle, 0};
    }
    return {_transient_buffer_handle, iter.value().offset};
}

void *ManagedDevice::get_native_handle(uint64_t handle) {
#ifndef NDEBUG
    if (!_is_committing) [[unlikely]] {
        LUISA_ERROR("Acquiring native handle out of commit scope.");
    }
#endif
    auto iter = _tex_desc_to_native.find(reinterpret_cast<ManagedTexDesc *>(handle));
    if (!iter) [[unlikely]] {
        return nullptr;
    }
    return iter.value().handle->res_info.native_handle;
}

void ManagedDevice::_mark_tex(uint64_t handle, uint64_t command_index) {
    auto iter = _tex_desc_to_native.find(reinterpret_cast<ManagedTexDesc *>(handle));
    if (!iter) return;
    auto &v = iter.value();
    v.start_command_index = std::min<int64_t>(v.start_command_index, command_index);
    v.end_command_index = std::max<int64_t>(v.end_command_index, command_index);
}

void ManagedDevice::_mark_buffer(uint64_t handle, uint64_t command_index) {
    auto iter = _buffer_desc_to_native.find(reinterpret_cast<size_t *>(handle));
    if (!iter) return;
    auto &v = iter.value();
    v.start_command_index = std::min<int64_t>(v.start_command_index, command_index);
    v.end_command_index = std::max<int64_t>(v.end_command_index, command_index);
}

void ManagedDevice::_preprocess(luisa::span<luisa::unique_ptr<Command> const> commands, CommandList &cmdlist) {
    for (uint64_t idx = 0; idx < commands.size(); ++idx) {
        auto cmd = commands[idx].get();
        switch (cmd->tag()) {

            case Command::Tag::EBufferToTextureCopyCommand: {
                auto c = static_cast<BufferToTextureCopyCommand const *>(cmd);
                _mark_buffer(c->buffer(), idx);
                _mark_tex(c->texture(), idx);
            } break;
            case Command::Tag::EShaderDispatchCommand: {
                auto c = static_cast<ShaderDispatchCommand const *>(cmd);
                for (auto &i : c->arguments()) {
                    switch (i.tag) {
                        case Argument::Tag::BUFFER:
                            _mark_buffer(i.buffer.handle, idx);
                            break;
                        case Argument::Tag::TEXTURE:
                            _mark_tex(i.texture.handle, idx);
                            break;
                    }
                }
            } break;
            case Command::Tag::ETextureUploadCommand: {
                auto c = static_cast<TextureUploadCommand const *>(cmd);
                _mark_tex(c->handle(), idx);
            } break;
            case Command::Tag::ETextureDownloadCommand: {
                auto c = static_cast<TextureUploadCommand const *>(cmd);
                _mark_tex(c->handle(), idx);
            } break;
            case Command::Tag::ETextureCopyCommand: {
                auto c = static_cast<TextureCopyCommand const *>(cmd);
                _mark_tex(c->src_handle(), idx);
                _mark_tex(c->dst_handle(), idx);
            } break;
            case Command::Tag::ETextureToBufferCopyCommand: {
                auto c = static_cast<TextureToBufferCopyCommand const *>(cmd);
                _mark_tex(c->texture(), idx);
                _mark_buffer(c->buffer(), idx);
            } break;
            case Command::Tag::EBufferUploadCommand: {
                auto c = static_cast<BufferUploadCommand const *>(cmd);
                _mark_buffer(c->handle(), idx);
            } break;
            case Command::Tag::EBufferDownloadCommand: {
                auto c = static_cast<BufferDownloadCommand const *>(cmd);
                _mark_buffer(c->handle(), idx);
            } break;
            case Command::Tag::EBufferCopyCommand: {
                auto c = static_cast<BufferCopyCommand const *>(cmd);
                _mark_buffer(c->src_handle(), idx);
                _mark_buffer(c->dst_handle(), idx);
            } break;
            case Command::Tag::EMeshBuildCommand: {
                auto c = static_cast<MeshBuildCommand const *>(cmd);
                _mark_buffer(c->vertex_buffer(), idx);
                _mark_buffer(c->triangle_buffer(), idx);
            } break;
            case Command::Tag::EProceduralPrimitiveBuildCommand: {
                auto c = static_cast<ProceduralPrimitiveBuildCommand const *>(cmd);
                _mark_buffer(c->aabb_buffer(), idx);
            } break;
            case Command::Tag::EBindlessArrayUpdateCommand:
            case Command::Tag::EAccelBuildCommand:
            case Command::Tag::ECurveBuildCommand:
            case Command::Tag::EMotionInstanceBuildCommand:
                break;
            case Command::Tag::ECustomCommand: {
                switch (static_cast<CustomCommand const *>(cmd)->custom_cmd_uuid()) {
                    case to_underlying(CustomCommandUUID::RASTER_CLEAR_DEPTH): {
                        auto c = static_cast<ClearDepthCommand const *>(cmd);
                        _mark_tex(c->handle(), idx);
                    } break;
                    case to_underlying(CustomCommandUUID::RASTER_CLEAR_RENDER_TARGET): {
                        auto c = static_cast<ClearRenderTargetCommand const *>(cmd);
                        _mark_tex(c->handle(), idx);
                    } break;
                    case to_underlying(CustomCommandUUID::RASTER_DRAW_SCENE): {
                        auto c = static_cast<DrawRasterSceneCommand const *>(cmd);
                        for (auto &i : c->arguments()) {
                            switch (i.tag) {
                                case Argument::Tag::BUFFER:
                                    _mark_buffer(i.buffer.handle, idx);
                                    break;
                                case Argument::Tag::TEXTURE:
                                    _mark_tex(i.texture.handle, idx);
                                    break;
                            }
                        }
                        for (auto &i : c->rtv_texs()) {
                            _mark_tex(i.handle, idx);
                        }
                        _mark_tex(c->dsv_tex().handle, idx);
                    } break;
                    case to_underlying(CustomCommandUUID::CUSTOM_DISPATCH): {
                        auto c = static_cast<CustomDispatchCommand const *>(cmd);
                        c->traverse_arguments([&]<typename T>(T const &t, auto usage) {
                            if constexpr (std::is_same_v<T, Argument::Buffer>) {
                                _mark_buffer(t.handle, idx);
                            } else if constexpr (std::is_same_v<T, Argument::Texture>) {
                                _mark_tex(t.handle, idx);
                            }
                        });
                    } break;
                    default:
                        LUISA_ERROR("Unsupported command.");
                }
            } break;
            default: {
                LUISA_ERROR("Unsupported command.");
            } break;
        }
    }
    _buffer_fit.clean_all();
    _command_caches.clear();
    _command_caches.resize(commands.size());
    for (auto &i : _tex_desc_to_native) {
        auto &v = i.second;
        if (v.start_command_index != std::numeric_limits<int64_t>::max() && v.end_command_index != std::numeric_limits<int64_t>::min()) {
            _command_caches[v.start_command_index]._allocate_tex.emplace_back(i.first, &i.second);
            _command_caches[v.end_command_index]._deallocate_tex.emplace_back(i.first, &i.second);
        }
    }
    for (auto &i : _buffer_desc_to_native) {
        auto &v = i.second;
        if (v.start_command_index != std::numeric_limits<int64_t>::max() && v.end_command_index != std::numeric_limits<int64_t>::min()) {
            _command_caches[v.start_command_index]._allocate_buffer.emplace_back(i.first, &i.second);
            _command_caches[v.end_command_index]._deallocate_buffer.emplace_back(i.first, &i.second);
        }
    }
    size_t buffer_size = 0;
    for (auto &caches : _command_caches) {
        for (auto alloc : caches._allocate_tex) {
            auto iter = _ready_texs.find(*alloc.first);
            auto managed_desc = alloc.second;
            if (iter && (!iter.value().empty())) {
                auto &vec = iter.value();
                auto res = vec.back();
                res->last_frame = _frame_index;
                vec.pop_back();
                managed_desc->handle = res;
            } else {
                managed_desc->handle = _allocate_handle(*alloc.first);
            }
        }
        for (auto dealloc : caches._deallocate_tex) {
            auto iter = _ready_texs.emplace(*dealloc.first);
            auto managed_desc = dealloc.second;
            LUISA_DEBUG_ASSERT(managed_desc->handle);
            iter.value().emplace_back(managed_desc->handle);
        }
        for (auto alloc : caches._allocate_buffer) {
            auto node = _buffer_fit.allocate_best_fit(*alloc.first);
            alloc.second->_node = node;
            alloc.second->offset = node->offset();
            buffer_size = std::max<size_t>(buffer_size, node->offset() + node->size());
        }
        for (auto dealloc : caches._deallocate_buffer) {
            _buffer_fit.free(dealloc.second->_node);
        }
    }
    buffer_size = (buffer_size + 65535ull) & (~65535ull);
    if (buffer_size > 0 && ((buffer_size > _transient_buffer_size || buffer_size <= _transient_buffer_size / 2))) {
        _transient_buffer_size = buffer_size;
        if (_transient_buffer_handle != invalid_resource_handle) {
            cmdlist.add_callback([handle = _transient_buffer_handle, impl = this->_impl]() {
                impl->destroy_buffer(handle);
            });
        }
        _transient_buffer_handle = _impl->create_buffer(Type::of<uint4>(), buffer_size / sizeof(uint4), nullptr).handle;
    }
    // steal command handles
    for (uint64_t idx = 0; idx < commands.size(); ++idx) {
        auto cmd = commands[idx].get();
        switch (cmd->tag()) {
            case Command::Tag::EBufferToTextureCopyCommand: {
                auto c = static_cast<BufferToTextureCopyCommand *>(cmd);
                c->set_texture_handle(_get_tex_handle(c->texture()));
                auto bf = _get_buffer_handle_offset(c->buffer());
                c->set_buffer_handle(bf.first);
                c->set_buffer_offset(c->buffer_offset() + bf.second);
            } break;
            case Command::Tag::EShaderDispatchCommand: {
                auto c = static_cast<ShaderDispatchCommand *>(cmd);
                for (auto &i : c->arguments()) {
                    switch (i.tag) {
                        case Argument::Tag::BUFFER: {
                            auto bf = _get_buffer_handle_offset(i.buffer.handle);
                            i.buffer.handle = bf.first;
                            i.buffer.offset += bf.second;
                        } break;
                        case Argument::Tag::TEXTURE: {
                            i.texture.handle = _get_tex_handle(i.texture.handle);
                        } break;
                    }
                }
            } break;
            case Command::Tag::ETextureUploadCommand: {
                auto c = static_cast<TextureUploadCommand *>(cmd);
                c->set_handle(_get_tex_handle(c->handle()));
            } break;
            case Command::Tag::ETextureDownloadCommand: {
                auto c = static_cast<TextureDownloadCommand *>(cmd);
                c->set_handle(_get_tex_handle(c->handle()));
            } break;
            case Command::Tag::ETextureCopyCommand: {
                auto c = static_cast<TextureCopyCommand *>(cmd);
                c->set_src_handle(_get_tex_handle(c->src_handle()));
                c->set_dst_handle(_get_tex_handle(c->dst_handle()));
            } break;
            case Command::Tag::ETextureToBufferCopyCommand: {
                auto c = static_cast<TextureToBufferCopyCommand *>(cmd);
                c->set_texture_handle(_get_tex_handle(c->texture()));
                auto bf = _get_buffer_handle_offset(c->buffer());
                c->set_buffer_handle(bf.first);
                c->set_buffer_offset(bf.second + c->buffer_offset());
            } break;
            case Command::Tag::EBufferUploadCommand: {
                auto c = static_cast<BufferUploadCommand *>(cmd);
                auto bf = _get_buffer_handle_offset(c->handle());
                c->set_handle(bf.first);
                c->set_offset(c->offset() + bf.second);
            } break;
            case Command::Tag::EBufferDownloadCommand: {
                auto c = static_cast<BufferDownloadCommand *>(cmd);
                _mark_buffer(c->handle(), idx);
                auto bf = _get_buffer_handle_offset(c->handle());
                c->set_handle(bf.first);
                c->set_offset(c->offset() + bf.second);
            } break;
            case Command::Tag::EBufferCopyCommand: {
                auto c = static_cast<BufferCopyCommand *>(cmd);
                _mark_buffer(c->src_handle(), idx);
                _mark_buffer(c->dst_handle(), idx);
                auto bf = _get_buffer_handle_offset(c->src_handle());
                c->set_src_handle(bf.first);
                c->set_src_offset(c->src_offset() + bf.second);

                bf = _get_buffer_handle_offset(c->dst_handle());
                c->set_dst_handle(bf.first);
                c->set_dst_offset(c->dst_offset() + bf.second);
            } break;
            case Command::Tag::EMeshBuildCommand: {
                auto c = static_cast<MeshBuildCommand *>(cmd);
                auto bf = _get_buffer_handle_offset(c->vertex_buffer());
                c->set_vertex_buffer(bf.first);
                c->set_vertex_offset(c->vertex_buffer_offset() + bf.second);

                bf = _get_buffer_handle_offset(c->triangle_buffer());
                c->set_triangle_buffer(bf.first);
                c->set_triangle_offset(c->triangle_buffer_offset() + bf.second);
            } break;
            case Command::Tag::EProceduralPrimitiveBuildCommand: {
                auto c = static_cast<ProceduralPrimitiveBuildCommand *>(cmd);
                auto bf = _get_buffer_handle_offset(c->aabb_buffer());
                c->set_aabb_buffer(bf.first);
                c->set_aabb_buffer_offset(c->aabb_buffer_offset() + bf.second);
            } break;
            case Command::Tag::ECustomCommand: {
                switch (static_cast<CustomCommand *>(cmd)->custom_cmd_uuid()) {
                    case to_underlying(CustomCommandUUID::RASTER_CLEAR_DEPTH): {
                        auto c = static_cast<ClearDepthCommand *>(cmd);
                    } break;
                    case to_underlying(CustomCommandUUID::RASTER_CLEAR_RENDER_TARGET): {
                        auto c = static_cast<ClearRenderTargetCommand *>(cmd);
                        c->set_handle(_get_tex_handle(c->handle()));
                    } break;
                    case to_underlying(CustomCommandUUID::RASTER_DRAW_SCENE): {
                        auto c = static_cast<DrawRasterSceneCommand *>(cmd);
                        for (auto &i : c->arguments()) {
                            switch (i.tag) {
                                case Argument::Tag::BUFFER: {
                                    auto bf = _get_buffer_handle_offset(i.buffer.handle);
                                    i.buffer.handle = bf.first;
                                    i.buffer.offset += bf.second;
                                } break;
                                case Argument::Tag::TEXTURE: {
                                    i.texture.handle = _get_tex_handle(i.texture.handle);
                                } break;
                            }
                        }
                        auto rtv_texs = c->rtv_texs();
                        for (auto &i : rtv_texs) {
                            i.handle = _get_tex_handle(i.handle);
                        }
                        auto dst_tex = c->dsv_tex();
                        dst_tex.handle = _get_tex_handle(dst_tex.handle);
                        c->set_dsv_texs(dst_tex);
                    } break;
                    case to_underlying(CustomCommandUUID::CUSTOM_DISPATCH): {
                        auto c = static_cast<CustomDispatchCommand *>(cmd);
                        c->traverse_arguments([&]<typename T>(T &t, auto usage) {
                            if constexpr (std::is_same_v<T, Argument::Buffer>) {
                                auto bf = _get_buffer_handle_offset(t.handle);
                                t.handle = bf.first;
                                t.offset += bf.second;
                            } else if constexpr (std::is_same_v<T, Argument::Texture>) {
                                t.handle = _get_tex_handle(t.handle);
                            }
                        });
                    } break;
                    default:
                        break;
                }
            } break;
            default:
                break;
        }
    }
}

BufferCreationInfo ManagedDevice::create_buffer(const ir::CArc<ir::Type> *element, size_t elem_count, void *external_memory /* nullptr if not imported from external memory */) noexcept {
    RBC_NOT_IMPL_RET;
}
BufferCreationInfo ManagedDevice::create_buffer(const Type *element, size_t elem_count, void *external_memory /* nullptr if not imported from external memory */) noexcept {
    if (_temp_name.empty()) [[unlikely]] {
        LUISA_ERROR("Texture must have name, call set_next_res_name first.");
    }
    if (!_is_managing) [[unlikely]] {
        LUISA_ERROR("Not in the managing scope, you can not allocate a texture .");
    }
    BufferCreationInfo info;
    info.element_stride = element->size();
    info.total_size_bytes = info.element_stride * elem_count;
    if (_next_is_only_get) {
        _next_is_only_get = false;
        auto iter = _buffer_name_to_desc.find(std::move(_temp_name));
        if (iter) {
            info.handle = reinterpret_cast<uint64_t>(iter.value());
        } else {
            info.handle = invalid_resource_handle;
        }
        info.native_handle = nullptr;
        return info;
    }
    auto iter =
        _buffer_name_to_desc
            .try_emplace(std::move(_temp_name), vstd::lazy_eval([&]() {
                             auto ptr = _buffer_meta_pool.create(info.total_size_bytes);
                             _buffer_desc_to_native.emplace(ptr);
                             return ptr;
                         }));
    luisa::string_view name_view{iter.first.key()};
    if (!iter.second) {
        auto src = iter.first.value();
        if (*src != info.total_size_bytes) [[unlikely]] {
            LUISA_ERROR("Trying to acquire buffer {} with different size.", name_view);
        }
    }
    info.handle = reinterpret_cast<uint64_t>(iter.first.value());
    info.native_handle = nullptr;
    return info;
}
// texture
ResourceCreationInfo ManagedDevice::create_texture(
    PixelFormat format, uint dimension,
    uint width, uint height, uint depth,
    uint mipmap_levels, void *external_native_handle,
    bool simultaneous_access, bool allow_raster_target) noexcept {
    if (_temp_name.empty()) [[unlikely]] {
        LUISA_ERROR("Texture must have name, call set_next_res_name first.");
    }
    if (!_is_managing) [[unlikely]] {
        LUISA_ERROR("Not in the managing scope, you can not allocate a texture .");
    }
    ManagedTexDesc desc{
        .format = format,
        .dimension = dimension,
        .width = width,
        .height = height,
        .depth = depth,
        .mipmap_levels = mipmap_levels,
        .mask = 0};
    desc.set_simultaneous_access(simultaneous_access);
    desc.set_allow_raster_target(allow_raster_target);
    if (_next_is_only_get) {
        _next_is_only_get = false;
        auto iter = _tex_name_to_desc.find(std::move(_temp_name));
        ResourceCreationInfo info;
        if (iter) {
            info.handle = reinterpret_cast<uint64_t>(iter.value());
        } else {
            info.handle = invalid_resource_handle;
        }
        info.native_handle = nullptr;
        return info;
    }
    auto iter =
        _tex_name_to_desc.try_emplace(std::move(_temp_name), vstd::lazy_eval([&]() {
                                          auto ptr = _tex_meta_pool.create(desc);
                                          _tex_desc_to_native.emplace(ptr);
                                          return ptr;
                                      }));
    luisa::string_view name_view{iter.first.key()};
    if (!iter.second) {
        auto src = iter.first.value();
        if (std::memcmp(src, &desc, sizeof(ManagedTexDesc)) != 0) [[unlikely]] {
            LUISA_ERROR("Trying to acquire texture {} with different description.", name_view);
        }
    }
    ResourceCreationInfo info;
    info.handle = reinterpret_cast<uint64_t>(iter.first.value());
    info.native_handle = nullptr;
    _temp_name.clear();
    return info;
}
void ManagedDevice::dispatch(uint64_t stream_handle, CommandList &&list) noexcept {
    auto commands = list.commands().subspan(managing_cmd_range.first, managing_cmd_range.second);
    _preprocess(commands, list);
    for (auto &i : _ready_texs) {
        auto &texs = i.second;
        for (size_t idx = 0; idx < texs.size(); ++idx) {
            if (texs[idx]->last_frame - _frame_index <= resource_contain_frame) continue;
            auto &v = texs[idx];
            _deallocate_handle(v);
            if (idx != texs.size() - 1) {
                v = texs.back();
            }
            texs.pop_back();
            --idx;
        }
    }
    if (!_tex_dispose_queue.empty()) {
        list.add_callback([impl = _impl, dsp = std::move(_tex_dispose_queue)]() {
            for (auto &i : dsp) {
                impl->destroy_texture(i);
            }
        });
    }
    _is_committing = true;
    _impl->dispatch(stream_handle, std::move(list));
    _is_committing = false;
    // finalize
    managing_cmd_range = {
        std::numeric_limits<uint64_t>::max(),
        std::numeric_limits<uint64_t>::max()};
    for (auto &i : _tex_desc_to_native) {
        _tex_meta_pool.destroy(i.first);
    }
    _tex_desc_to_native.clear();
    _tex_name_to_desc.clear();
    _buffer_name_to_desc.clear();
    _temp_name.clear();
    _buffer_meta_pool.destroy_all();
    _buffer_desc_to_native.clear();
    ++_frame_index;
}
vstd::string ManagedDevice::log_resource_info() {
    vstd::string result;
    for (auto &i : _tex_name_to_desc) {
        auto &&v = i.second;
        auto size = v->dimension == 2 ? luisa::format("({}, {})", v->width, v->height) : luisa::format("({}, {}, {})", v->width, v->height, v->depth);
        result += luisa::format("Texture '{}' format {}, size {}, mipmap_levels {}, simultaneous_access {}, allow_raster_target {}\n", i.first, luisa::to_string(v->format), size, v->mipmap_levels, v->simultaneous_access(), v->allow_raster_target());
    }
    for (auto &i : _buffer_name_to_desc) {
        result += luisa::format("Buffer '{}', size {}\n", i.first, *i.second);
    }
    return result;
}

//////////////// Others
void *ManagedDevice::native_handle() const noexcept {
    return _impl->native_handle();
}
uint ManagedDevice::compute_warp_size() const noexcept {
    return _impl->compute_warp_size();
}
uint64_t ManagedDevice::memory_granularity() const noexcept {
    return _impl->memory_granularity();
}
void ManagedDevice::destroy_texture(uint64_t handle) noexcept {
    // Managed resource can not be free manually
}
void ManagedDevice::destroy_buffer(uint64_t handle) noexcept {
    // Managed resource can not be free manually
}
// bindless array
ResourceCreationInfo ManagedDevice::create_bindless_array(size_t size, BindlessSlotType type) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_bindless_array(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

// stream
ResourceCreationInfo ManagedDevice::create_stream(StreamTag stream_tag) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_stream(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::synchronize_stream(uint64_t stream_handle) noexcept {
    RBC_NOT_IMPL;
}

using StreamLogCallback = luisa::function<void(luisa::string_view)>;
void ManagedDevice::set_stream_log_callback(uint64_t stream_handle, const StreamLogCallback &callback) noexcept {
    RBC_NOT_IMPL;
}

// swap chain
SwapchainCreationInfo ManagedDevice::create_swapchain(const SwapchainOption &option, uint64_t stream_handle) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_swapchain(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::present_display_in_stream(uint64_t stream_handle, uint64_t swapchain_handle, uint64_t image_handle) noexcept {
    RBC_NOT_IMPL;
}

// kernel
ShaderCreationInfo ManagedDevice::create_shader(const ShaderOption &option, Function kernel) noexcept {
    RBC_NOT_IMPL_RET;
}
ShaderCreationInfo ManagedDevice::create_shader(const ShaderOption &option, const ir::KernelModule *kernel) noexcept {
    RBC_NOT_IMPL_RET;
}
ShaderCreationInfo ManagedDevice::create_shader(const ShaderOption &option, const ir_v2::KernelModule &kernel) noexcept {
    RBC_NOT_IMPL_RET;
}
ShaderCreationInfo ManagedDevice::load_shader(luisa::string_view name, luisa::span<const Type *const> arg_types) noexcept {
    RBC_NOT_IMPL_RET;
}
Usage ManagedDevice::shader_argument_usage(uint64_t handle, size_t index) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_shader(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

// event
ResourceCreationInfo ManagedDevice::create_event() noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_event(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::signal_event(uint64_t handle, uint64_t stream_handle, uint64_t fence_value) noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::wait_event(uint64_t handle, uint64_t stream_handle, uint64_t fence_value) noexcept {
    RBC_NOT_IMPL;
}
bool ManagedDevice::is_event_completed(uint64_t handle, uint64_t fence_value) const noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::synchronize_event(uint64_t handle, uint64_t fence_value) noexcept {
    RBC_NOT_IMPL;
}

// accel
ResourceCreationInfo ManagedDevice::create_mesh(const AccelOption &option) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_mesh(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

ResourceCreationInfo ManagedDevice::create_procedural_primitive(const AccelOption &option) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_procedural_primitive(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

ResourceCreationInfo ManagedDevice::create_curve(const AccelOption &option) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_curve(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

ResourceCreationInfo ManagedDevice::create_motion_instance(const AccelMotionOption &option) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_motion_instance(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

ResourceCreationInfo ManagedDevice::create_accel(const AccelOption &option) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_accel(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

// query
luisa::string ManagedDevice::query(luisa::string_view property) noexcept {
    RBC_NOT_IMPL_RET;
}
DeviceExtension *ManagedDevice::extension(luisa::string_view name) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::set_name(luisa::compute::Resource::Tag resource_tag, uint64_t resource_handle, luisa::string_view name) noexcept {
    RBC_NOT_IMPL;
}
luisa::string_view ManagedDevice::get_name(uint64_t resource_handle) const noexcept {
    RBC_NOT_IMPL_RET;
}

// sparse buffer
SparseBufferCreationInfo ManagedDevice::create_sparse_buffer(const Type *element, size_t elem_count) noexcept {
    RBC_NOT_IMPL_RET;
}
ResourceCreationInfo ManagedDevice::allocate_sparse_buffer_heap(size_t byte_size) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::deallocate_sparse_buffer_heap(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::update_sparse_resources(
    uint64_t stream_handle,
    luisa::vector<SparseUpdateTile> &&textures_update) noexcept {
    RBC_NOT_IMPL;
}
void ManagedDevice::destroy_sparse_buffer(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}

// sparse texture
ResourceCreationInfo ManagedDevice::allocate_sparse_texture_heap(size_t byte_size) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::deallocate_sparse_texture_heap(uint64_t handle) noexcept {}
SparseTextureCreationInfo ManagedDevice::create_sparse_texture(
    PixelFormat format, uint dimension,
    uint width, uint height, uint depth,
    uint mipmap_levels, bool simultaneous_access) noexcept {
    RBC_NOT_IMPL_RET;
}
void ManagedDevice::destroy_sparse_texture(uint64_t handle) noexcept {
    RBC_NOT_IMPL;
}
}// namespace rbc
#undef RBC_NOT_IMPL
#undef RBC_NOT_IMPL_RET
