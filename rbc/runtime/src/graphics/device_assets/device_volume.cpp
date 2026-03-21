#include <rbc_graphics/device_assets/device_volume.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_io/io_command_list.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
Volume<float> const &DeviceVolume::get_float_volume() const {
    LUISA_ASSUME(_vol.index() == luisa::to_underlying(VolumeType::Float));
    return _vol.get<luisa::to_underlying(VolumeType::Float)>();
}
Volume<int> const &DeviceVolume::get_int_volume() const {
    LUISA_ASSUME(_vol.index() == luisa::to_underlying(VolumeType::Int));
    return _vol.get<luisa::to_underlying(VolumeType::Int)>();
}
Volume<uint> const &DeviceVolume::get_uint_volume() const {
    LUISA_ASSUME(_vol.index() == luisa::to_underlying(VolumeType::UInt));
    return _vol.get<luisa::to_underlying(VolumeType::UInt)>();
}
DeviceVolume::DeviceVolume() {}
DeviceVolume::~DeviceVolume() {
    if (!_vol.valid()) return;
    auto inst = AssetsManager::instance();
    if (inst) {
        inst->dispose_after_render_frame(std::move(_vol));
        if (_heap_idx != ~0u) {
            inst->scene_mng()->bindless_allocator().deallocate_tex3d(_heap_idx);
        }
    }
}
uint DeviceVolume::_check_size(PixelStorage storage, uint3 size, uint desire_mip) {
    auto dst_mip_level = 0;
    auto mip_size = size;
    auto min_size = is_block_compressed(storage) ? 4 : 1;
    for (auto i [[maybe_unused]] : vstd::range(desire_mip)) {
        if (any(mip_size < 1u)) {
            break;
        }
        dst_mip_level = i + 1;
        mip_size >>= 1u;
        mip_size = max(mip_size, uint3(min_size));
    }
    return dst_mip_level;
}
template<typename T, typename ErrFunc>
void DeviceVolume::_create_vol(Volume<T> &vol, PixelStorage storage, uint3 size, uint mip_level, uint &dst_mip_level, ErrFunc &&err_func) {
    auto inst = AssetsManager::instance();
    dst_mip_level = _check_size(storage, size, mip_level);
    if (dst_mip_level == 0) {
        err_func();
    }
    if (vol && (!(all(vol.size() == size)) && vol.storage() == storage && vol.mip_levels() == dst_mip_level)) {
        inst->dispose_after_render_frame(std::move(vol));
    }
    if (!vol)
        vol = inst->lc_device().create_volume<T>(storage, size, dst_mip_level);
}
template<typename LoadType>
void DeviceVolume::_async_load(
    Sampler sampler,
    PixelStorage storage,
    uint3 size,
    uint mip_level,
    VolumeType volume_type,
    LoadType &&load_type,
    bool copy_to_memory) {
    if (luisa::to_underlying(volume_type) >= luisa::to_underlying(VolumeType::None)) [[unlikely]] {
        LUISA_ERROR("Bad VolumeType.");
    }
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    uint dst_mip_level = 0;
    auto create_func = [&]<typename T>(Volume<T> &vol) {
        _create_vol(vol, storage, size, mip_level, dst_mip_level, [&]() {
            LUISA_ERROR("Volume format invalid.");
        });
    };
    switch (volume_type) {
        case VolumeType::Float: {
            if (_vol.index() != 0) {
                _vol = Volume<float>();
            }
            create_func(_vol.force_get<Volume<float>>());
            _create_heap_idx();
        } break;
        case VolumeType::Int: {
            if (_vol.index() != 1) {
                _vol = Volume<int>();
            }
            create_func(_vol.force_get<Volume<int>>());
        } break;
        case VolumeType::UInt: {
            if (_vol.index() != 2) {
                _vol = Volume<uint>();
            }
            create_func(_vol.force_get<Volume<uint>>());
        } break;
        default:
            break;
    }
    inst->load_thd_queue.push(
        [this_shared = RC{this}, copy_to_memory, load_type = std::move(load_type), volume_type, dst_mip_level](
            LoadTaskArgs const &args) mutable {
            (void)copy_to_memory;
            auto ptr = static_cast<DeviceVolume *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            auto func = [&]<typename T>(Volume<T> &vol) {
                /////////// Load as file
                if constexpr (std::is_same_v<LoadType, FileLoad>) {
                    luisa::filesystem::path const &path = load_type.path;
                    size_t file_offset = load_type.file_offset;
                    auto file = args.io_cmdlist.retrieve(luisa::to_string(path));
                    auto file_end = file.length();

                    size_t offset = file_offset;
                    for (auto i : vstd::range(dst_mip_level)) {
                        auto vol_view = vol.view(i);
                        if (copy_to_memory) {
                            *args.require_disk_io_sync = true;
                            inst->load_tex_uploader().upload_with_copy(
                                args.io_cmdlist,
                                args.mem_io_cmdlist,
                                this_shared->_host_data,
                                args.cmdlist,
                                args.device,
                                *args.disp_queue,
                                file,
                                reinterpret_cast<ImageView<float> &>(vol_view),// layout is same
                                offset);
                        } else {
                            inst->load_tex_uploader().upload(
                                args.io_cmdlist,
                                args.cmdlist,
                                args.device,
                                *args.disp_queue,
                                file,
                                reinterpret_cast<ImageView<float> &>(vol_view),// layout is same
                                offset);
                        }
                        offset += vol_view.size_bytes();
                        if (offset > file_end) [[unlikely]] {
                            LUISA_ERROR("File less than volume desire size.");
                        }
                    }
                }
                /////////// Load as memory
                else if constexpr (std::is_same_v<LoadType, MemoryLoad>) {
                    BinaryBlob &data = load_type.blob;
                    size_t offset = 0;
                    for (auto i : vstd::range(dst_mip_level)) {
                        auto vol_view = vol.view(i);
                        inst->load_tex_uploader().upload(
                            args.mem_io_cmdlist,
                            args.cmdlist,
                            args.device,
                            *args.disp_queue,
                            data.data() + offset,
                            reinterpret_cast<ImageView<float> &>(vol_view));
                        offset += vol_view.size_bytes();
                    }
                    args.disp_queue->dispose_after_queue(std::move(data));
                }
                /////////// Load as Buffer
                else if constexpr (std::is_same_v<LoadType, BufferViewLoad>) {
                    BufferView<uint> buffer = load_type.buffer;
                    size_t offset = 0;
                    for (auto i : vstd::range(dst_mip_level)) {
                        auto vol_view = vol.view(i);
                        inst->load_tex_uploader().copy(
                            args.cmdlist,
                            buffer.subview(offset / sizeof(uint), vol_view.size_bytes() / sizeof(uint)),
                            reinterpret_cast<ImageView<float> &>(vol_view));
                        offset += vol_view.size_bytes();
                    }
                } else {
                    static_assert(luisa::always_false_v<LoadType>, "Illegal type.");
                }
            };
            switch (volume_type) {
                case VolumeType::Float: {
                    auto &vol = ptr->_vol.template get<0>();
                    func.template operator()<float>(vol);
                } break;
                case VolumeType::Int: {
                    auto &vol = ptr->_vol.template get<1>();
                    func.template operator()<int>(vol);
                } break;
                case VolumeType::UInt: {
                    auto &vol = ptr->_vol.template get<2>();
                    func.template operator()<uint>(vol);
                } break;
                default:
                    break;
            }
        });
}

void DeviceVolume::_create_heap_idx() {
    auto &sm = SceneManager::instance();
    if (_heap_idx != ~0u)
        sm.bindless_allocator().deallocate_tex3d(_heap_idx);
    _heap_idx = sm.bindless_allocator().allocate_tex3d(_vol.force_get<Volume<float>>(), Sampler{});
}

void DeviceVolume::async_load_from_file(
    luisa::filesystem::path const &path,
    size_t file_offset,
    Sampler sampler,
    PixelStorage storage,
    uint3 size,
    uint mip_level,
    VolumeType volume_type,
    bool copy_to_host) {
    _async_load(sampler, storage, size, mip_level, volume_type, FileLoad{path, file_offset}, copy_to_host);
}

void DeviceVolume::async_load_from_buffer(
    BufferView<uint> buffer,
    Sampler sampler,
    PixelStorage storage,
    uint3 size,
    uint mip_level,
    VolumeType volume_type) {
    _async_load(sampler, storage, size, mip_level, volume_type, BufferViewLoad{buffer});
}
void DeviceVolume::async_load_from_memory(
    BinaryBlob &&data,
    Sampler sampler,
    PixelStorage storage,
    uint3 size,
    uint mip_level,
    VolumeType volume_type,
    bool copy_to_host) {
    if (copy_to_host) {
        if (data.data()) {
            if (_host_data.size() != data.size()) {
                _host_data.clear();
                _host_data.push_back_uninitialized(data.size());
            }
            std::memcpy(_host_data.data(), data.data(), data.size());
        }
        data = BinaryBlob{
            _host_data.data(),
            _host_data.size(),
            {}};
    }
    _async_load(sampler, storage, size, mip_level, volume_type, MemoryLoad{std::move(data)});
}
}// namespace rbc
