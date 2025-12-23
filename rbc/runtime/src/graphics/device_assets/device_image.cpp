#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_io/io_command_list.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
Image<float> const &DeviceImage::get_float_image() const {
    LUISA_ASSUME(_img.index() == luisa::to_underlying(ImageType::Float));
    return _img.get<luisa::to_underlying(ImageType::Float)>();
}
Image<int> const &DeviceImage::get_int_image() const {
    LUISA_ASSUME(_img.index() == luisa::to_underlying(ImageType::Int));
    return _img.get<luisa::to_underlying(ImageType::Int)>();
}
Image<uint> const &DeviceImage::get_uint_image() const {
    LUISA_ASSUME(_img.index() == luisa::to_underlying(ImageType::UInt));
    return _img.get<luisa::to_underlying(ImageType::UInt)>();
}
DeviceImage::DeviceImage() {}
DeviceImage::~DeviceImage() {
    if (!_img.valid()) return;
    auto inst = AssetsManager::instance();
    if (inst) {
        inst->dispose_after_render_frame(std::move(_img));
        if (_heap_idx != ~0u) {
            inst->scene_mng()->bindless_allocator().deallocate_tex2d(_heap_idx);
        }
    }
}
uint DeviceImage::_check_size(PixelStorage storage, uint2 size, uint desire_mip) {
    auto dst_mip_level = 0;
    auto mip_size = size;
    uint64_t offset = 0;
    auto min_size = is_block_compressed(storage) ? 4 : 1;
    for (auto i : vstd::range(desire_mip)) {
        if (any(mip_size < 1u)) {
            break;
        }
        offset += pixel_storage_size(storage, make_uint3(mip_size, 1));
        dst_mip_level = i + 1;
        mip_size >>= 1u;
        mip_size = max(mip_size, uint2(1));
    }
    return dst_mip_level;
}
template<typename T, typename ErrFunc>
void DeviceImage::_create_img(Image<T> &img, PixelStorage storage, uint2 size, uint mip_level, uint &dst_mip_level, ErrFunc &&err_func) {
    auto inst = AssetsManager::instance();
    // file.length() - file_offset
    dst_mip_level = _check_size(storage, size, mip_level);
    if (dst_mip_level == 0) {
        err_func();
    }
    if (img && (!(all(img.size() == size)) && img.storage() == storage && img.mip_levels() == dst_mip_level)) {
        inst->dispose_after_render_frame(std::move(img));
    }
    if (!img)
        img = inst->lc_device().create_image<T>(storage, size, dst_mip_level);
}
template<typename LoadType>
void DeviceImage::_async_load(
    Sampler sampler,
    PixelStorage storage,
    uint2 size,
    uint mip_level,
    ImageType image_type,
    LoadType &&load_type,
    bool copy_to_memory) {
    if (luisa::to_underlying(image_type) >= luisa::to_underlying(ImageType::None)) [[unlikely]] {
        LUISA_ERROR("Bad ImageType.");
    }
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    uint dst_mip_level = 0;
    auto create_func = [&]<typename T>(Image<T> &img) {
        _create_img(img, storage, size, mip_level, dst_mip_level, [&]() {
            LUISA_ERROR("Texture format invalid.");
        });
    };
    switch (image_type) {
        case ImageType::Float: {
            if (_img.index() != 0) {
                _img = Image<float>();
            }
            create_func(_img.force_get<Image<float>>());
            _create_heap_idx();
        } break;
        case ImageType::Int: {
            if (_img.index() != 1) {
                _img = Image<int>();
            }
            create_func(_img.force_get<Image<int>>());
        } break;
        case ImageType::UInt: {
            if (_img.index() != 2) {
                _img = Image<uint>();
            }
            create_func(_img.force_get<Image<uint>>());
        } break;
        default:
            break;
    }
    inst->load_thd_queue.push(
        [this_shared = RC{this}, copy_to_memory, load_type = std::move(load_type), image_type, dst_mip_level](
            LoadTaskArgs const &args) mutable {
            auto ptr = static_cast<DeviceImage *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            auto func = [&]<typename T>(Image<T> &img) {
                /////////// Load as file
                if constexpr (std::is_same_v<LoadType, FileLoad>) {
                    luisa::filesystem::path const &path = load_type.path;
                    size_t file_offset = load_type.file_offset;
                    auto file = args.io_cmdlist.retrieve(luisa::to_string(path));
                    auto file_end = file.length();

                    size_t offset = file_offset;
                    for (auto i : vstd::range(dst_mip_level)) {
                        auto img_view = img.view(i);
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
                                reinterpret_cast<ImageView<float> &>(img_view),// layout is same
                                offset);
                        } else {
                            inst->load_tex_uploader().upload(
                                args.io_cmdlist,
                                args.cmdlist,
                                args.device,
                                *args.disp_queue,
                                file,
                                reinterpret_cast<ImageView<float> &>(img_view),// layout is same
                                offset);
                        }
                        offset += img_view.size_bytes();
                        if (offset > file_end) [[unlikely]] {
                            LUISA_ERROR("File less than texture desire size.");
                        }
                    }
                }
                /////////// Load as memory
                else if constexpr (std::is_same_v<LoadType, MemoryLoad>) {
                    BinaryBlob &data = load_type.blob;
                    size_t offset = 0;
                    for (auto i : vstd::range(dst_mip_level)) {
                        auto img_view = img.view(i);
                        inst->load_tex_uploader().upload(
                            args.mem_io_cmdlist,
                            args.cmdlist,
                            args.device,
                            *args.disp_queue,
                            data.data() + offset,
                            reinterpret_cast<ImageView<float> &>(img_view));
                        offset += img_view.size_bytes();
                    }
                    args.disp_queue->dispose_after_queue(std::move(data));
                }
                /////////// Load as Buffer
                else if constexpr (std::is_same_v<LoadType, BufferViewLoad>) {
                    BufferView<uint> buffer = load_type.buffer;
                    size_t offset = 0;
                    for (auto i : vstd::range(dst_mip_level)) {
                        auto img_view = img.view(i);
                        inst->load_tex_uploader().copy(
                            args.cmdlist,
                            buffer.subview(offset / sizeof(uint), img_view.size_bytes() / sizeof(uint)),
                            reinterpret_cast<ImageView<float> &>(img_view));
                        ;
                        offset += img_view.size_bytes();
                    }
                } else {
                    static_assert(luisa::always_false_v<LoadType>, "Illegal type.");
                }
            };
            switch (image_type) {
                case ImageType::Float: {
                    auto &img = ptr->_img.template get<0>();
                    func.template operator()<float>(img);
                } break;
                case ImageType::Int: {
                    auto &img = ptr->_img.template get<1>();
                    func.template operator()<int>(img);
                } break;
                case ImageType::UInt: {
                    auto &img = ptr->_img.template get<2>();
                    func.template operator()<uint>(img);
                } break;
                default:
                    break;
            }
        });
}

void DeviceImage::_create_heap_idx() {
    auto &sm = SceneManager::instance();
    if (_heap_idx != ~0u)
        sm.bindless_allocator().deallocate_tex2d(_heap_idx);
    _heap_idx = sm.bindless_allocator().allocate_tex2d(_img.force_get<Image<float>>(), Sampler{});
}

void DeviceImage::async_load_from_file(
    luisa::filesystem::path const &path,
    size_t file_offset,
    Sampler sampler,
    PixelStorage storage,
    uint2 size,
    uint mip_level,
    ImageType image_type,
    bool copy_to_host) {
    _async_load(sampler, storage, size, mip_level, image_type, FileLoad{path, file_offset}, copy_to_host);
}

void DeviceImage::async_load_from_buffer(
    BufferView<uint> buffer,
    Sampler sampler,
    PixelStorage storage,
    uint2 size,
    uint mip_level,
    ImageType image_type) {
    _async_load(sampler, storage, size, mip_level, image_type, BufferViewLoad{buffer});
}
void DeviceImage::async_load_from_memory(
    BinaryBlob &&data,
    Sampler sampler,
    PixelStorage storage,
    uint2 size,
    uint mip_level,
    ImageType image_type,
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
    _async_load(sampler, storage, size, mip_level, image_type, MemoryLoad{std::move(data)});
}
}// namespace rbc