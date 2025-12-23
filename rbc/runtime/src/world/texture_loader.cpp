#pragma once
#include <rbc_world/resources/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world/type_register.h>
#include <stb/stb_image.h>
#include <tinytiffreader.h>
#include <tinyexr.h>
#include <rbc_world/texture_loader.h>

namespace rbc::world {
void TextureLoader::_try_execute() {
    std::lock_guard lck{_mtx};
    while (true) {
        auto v = _after_device_task.front();
        if (v && _event.is_completed(v->second)) {
            luisa::fiber::schedule([counter = _counter, func = std::move(v->first)]() {
                func();
                counter.done();
            });
            _after_device_task.pop_discard();
            continue;
        }
        break;
    }
}
RC<TextureResource> TextureLoader::decode_texture(
    luisa::filesystem::path const &path,
    uint mip_level,
    bool to_vt) {

    auto ext = luisa::to_string(path.extension());
    for (auto &i : ext) {
        i = std::tolower(i);
    }
    if (!luisa::filesystem::exists(path)) return {};
    uint64_t gpu_fence = 0;
    auto to_vt_func = [&](RC<TextureResource> const &tex, uint chunk_size) {
        if (!to_vt) return;
        if (all((tex->size() & (chunk_size - 1u)) != 0u)) {
            LUISA_WARNING("Texture size {} is not aligned as {}", tex->size(), chunk_size);
            return;
        }
        if (gpu_fence > 0) {
            _counter.add();
            std::lock_guard lck{_mtx};
            _after_device_task.push(
                [tex]() {
                    tex->pack_to_tile();
                },
                gpu_fence);
        } else {
            tex->pack_to_tile();
        }
    };
    auto process_tex = [&](RC<TextureResource> const &tex) {
        auto render_device = RenderDevice::instance_ptr();
        LUISA_ASSERT(render_device, "Render device must be initialized.");
        {
            std::lock_guard lck{_mtx};
            if (!_pack_tex) {
                _pack_tex.create(render_device->lc_device());
            }
            if (!_event) {
                _event = render_device->lc_device().create_timeline_event();
            }
        }
        uint chunk_size = TexStreamManager::chunk_resolution;
        if (is_block_compressed((PixelStorage)tex->pixel_storage())) {
            chunk_size /= 4;
        }
        if (mip_level > 1) {
            auto mip_size = tex->size();
            auto desire_mip_level = 0;
            if (!to_vt) {
                for (auto i : vstd::range(mip_level)) {
                    desire_mip_level++;
                    if (any(mip_size <= 64u)) {
                        break;
                    }
                    mip_size >>= 1u;
                }
            } else {
                for (auto i : vstd::range(mip_level)) {
                    if (any((mip_size & (chunk_size - 1u)) != 0u)) {
                        break;
                    }
                    desire_mip_level++;
                    if (any(mip_size <= chunk_size)) {
                        break;
                    }
                    mip_size >>= 1u;
                }
            }
            mip_level = desire_mip_level;
        }
        mip_level = std::max<uint>(mip_level, 1);
        tex->_mip_level = mip_level;
        if (mip_level > 1) {
            std::unique_lock lck{_mtx};
            if (_cmdlist.commands().size() > 64) {
                auto fence = _finished_fence++;
                render_device->lc_main_stream()
                    << _cmdlist.commit()
                    << _event.signal(fence);
                lck.unlock();
                if (fence > 2) {
                    _event.synchronize(fence - 2);
                    _try_execute();
                }
                lck.lock();
            }
            lck.unlock();
            uint64_t size = 0;
            for (auto i : vstd::range(mip_level)) {
                size += pixel_storage_size((PixelStorage)tex->pixel_storage(), make_uint3(tex->size() >> (uint)i, 1));
            }
            auto &host_data = *tex->host_data();
            host_data.resize_uninitialized(size);
            tex->init_device_resource();
            auto &&img = tex->get_image()->get_float_image();
            lck.lock();
            gpu_fence = _finished_fence.load();
            uint64_t offset = 0;
            {
                auto view = img.view(0);
                auto level_size = view.size_bytes();
                _cmdlist << view.copy_from(host_data.data());
                offset += level_size;
            }
            _pack_tex->generate_mip(
                _cmdlist,
                img);

            for (auto i : vstd::range(1, mip_level)) {
                auto view = img.view(i);
                auto level_size = view.size_bytes();
                _cmdlist << view.copy_to(host_data.data() + offset);
                offset += level_size;
            }
        }

        to_vt_func(tex, chunk_size);
    };
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" || ext == ".psd" || ext == ".pnm" || ext == ".tga") {
        BinaryFileStream file_stream(luisa::to_string(path));
        if (!file_stream.valid()) return {};
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        int x;
        int y;
        int channels_in_file;
        auto ptr = stbi_load_from_memory(
            (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
        auto tex = new DeviceImage();

        RC<TextureResource> result{world::create_object<TextureResource>()};

        result->_tex = tex;
        auto &img = tex->host_data_ref();
        result->_size = uint2(x, y);
        img.push_back_uninitialized(x * y * sizeof(stbi_uc) * 4);
        std::memcpy(img.data(), ptr, img.size_bytes());
        stbi_image_free(ptr);
        result->_pixel_storage = LCPixelStorage::BYTE4;
        result->_mip_level = 1;// TODO: generate mip
        result->_is_vt = false;// TODO: support virtual texture
        process_tex(result);
        return result;
    }
    if (ext == ".hdr") {
        BinaryFileStream file_stream(luisa::to_string(path));
        if (!file_stream.valid()) return {};
        luisa::vector<std::byte> data;
        data.push_back_uninitialized(file_stream.length());
        file_stream.read(data);
        int x;
        int y;
        int channels_in_file;
        auto ptr = stbi_loadf_from_memory(
            (const stbi_uc *)data.data(), data.size(), &x, &y, &channels_in_file, 4);
        auto tex = new DeviceImage();
        RC<TextureResource> result{world::create_object<TextureResource>()};
        result->_tex = tex;
        auto &img = tex->host_data_ref();
        result->_size = uint2(x, y);
        img.push_back_uninitialized(x * y * sizeof(float) * 4);
        std::memcpy(img.data(), ptr, img.size_bytes());
        stbi_image_free(ptr);
        result->_pixel_storage = LCPixelStorage::FLOAT4;
        result->_mip_level = 1;// TODO: generate mip
        result->_is_vt = false;// TODO: support virtual texture
        process_tex(result);
        return result;
    }
    if (ext == ".tiff") {
        auto tiffr = TinyTIFFReader_open(luisa::to_string(path).c_str());
        if (!tiffr) return {};
        auto dsp = vstd::scope_exit([&] {
            TinyTIFFReader_close(tiffr);
        });
        if (TinyTIFFReader_wasError(tiffr)) {
            LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
            return {};
        }
        const uint16_t samples = TinyTIFFReader_getSamplesPerPixel(tiffr);
        const uint16_t bitspersample = TinyTIFFReader_getBitsPerSample(tiffr, 0);
        auto width = TinyTIFFReader_getWidth(tiffr);
        auto height = TinyTIFFReader_getHeight(tiffr);
        PixelStorage pixel_storage;
        switch (bitspersample) {
            case 8:
                pixel_storage = PixelStorage::BYTE1;
                break;
            case 16:
                pixel_storage = PixelStorage::SHORT1;
                break;
            case 32:
                pixel_storage = PixelStorage::INT1;
                break;
            default:
                LUISA_WARNING("Unsupported bits-per-sample");
                return {};
        }
        uint channel_rate = 1;
        switch (samples) {
            case 1:
                break;
            case 2:
                pixel_storage = (PixelStorage)(luisa::to_underlying(pixel_storage) + 1);
                channel_rate = 2;
                break;
            case 3:
            case 4:
                pixel_storage = (PixelStorage)(luisa::to_underlying(pixel_storage) + 2);
                channel_rate = 4;
                break;
            default:
                LUISA_WARNING("Unsupported sample count");
                return {};
        }
        RC<TextureResource> result{world::create_object<TextureResource>()};
        result->_pixel_storage = (LCPixelStorage)pixel_storage;
        result->_size = uint2(width, height);
        result->_mip_level = 1;// TODO: mipmap generate
        result->_is_vt = false;
        luisa::vector<std::byte> image;
        image.push_back_uninitialized(width * height * bitspersample / 8);
        if (samples == 1) {
            TinyTIFFReader_getSampleData(tiffr, image.data(), 0);
            if (TinyTIFFReader_wasError(tiffr)) {
                LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
                return {};
            }
            auto tex = new DeviceImage();
            result->_tex = tex;
            tex->host_data_ref() = std::move(image);
            process_tex(result);
            return result;
        }
        auto tex = new DeviceImage();
        result->_tex = tex;
        tex->host_data_ref().resize_uninitialized(pixel_storage_size(pixel_storage, make_uint3(result->_size, 1)));
        auto pixel_count = width * height;
        for (uint16_t sample = 0; sample < samples; sample++) {
            // read the sample
            TinyTIFFReader_getSampleData(tiffr, image.data(), sample);
            if (TinyTIFFReader_wasError(tiffr)) {
                LUISA_WARNING("{}", TinyTIFFReader_getLastError(tiffr));
                result->_tex.reset();
                return {};
            }
            switch (bitspersample) {
                case 8: {
                    auto ptr = (uint8_t *)tex->host_data_ref().data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + sample] = (uint8_t)image[i];
                    }
                } break;
                case 16: {
                    auto ptr = (uint16_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint16_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + sample] = image_ptr[i];
                    }
                } break;
                default: {
                    auto ptr = (uint32_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint32_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + sample] = image_ptr[i];
                    }
                } break;
            }
        }
        // make last zero
        if (samples == 3) {
            switch (bitspersample) {
                case 8: {
                    auto ptr = (uint8_t *)tex->host_data_ref().data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + 3] = (uint8_t)image[i];
                    }
                } break;
                case 16: {
                    auto ptr = (uint16_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint16_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + 3] = image_ptr[i];
                    }
                } break;
                default: {
                    auto ptr = (uint32_t *)tex->host_data_ref().data();
                    auto image_ptr = (uint32_t *)image.data();
                    for (uint i = 0; i < pixel_count; ++i) {
                        ptr[i * channel_rate + 3] = image_ptr[i];
                    }
                } break;
            }
        }
        process_tex(result);
        return result;
    }
    if (ext == ".exr") {
        float *out;// width * height * RGBA
        int width;
        int height;
        const char *err = nullptr;
        int ret = LoadEXR(&out, &width, &height, luisa::to_string(path).c_str(), &err);
        if (ret != TINYEXR_SUCCESS) {
            if (err) {
                LUISA_WARNING("{}", err);
                FreeEXRErrorMessage(err);// release memory of error message.
                return {};
            }
        }
        auto tex = new DeviceImage();
        RC<TextureResource> result{world::create_object<TextureResource>()};
        result->_tex = tex;
        auto &img = tex->host_data_ref();
        result->_size = make_uint2(width, height);
        img.push_back_uninitialized(width * height * sizeof(float) * 4);
        std::memcpy(img.data(), out, img.size_bytes());
        free(out);
        result->_pixel_storage = LCPixelStorage::FLOAT4;
        result->_mip_level = 1;// TODO: generate mip
        result->_is_vt = false;// TODO: support virtual texture
        process_tex(result);
        return result;
    }
    return {};
}

void TextureResource::_pack_to_tile_level(uint level, luisa::span<std::byte const> src, luisa::span<std::byte> dst) {
    auto size = _size >> level;
    uint64_t pixel_size_bytes;
    uint64_t raw_size_bytes;
    uint chunk_resolution = TexStreamManager::chunk_resolution;
    if (is_block_compressed((PixelStorage)_pixel_storage)) {
        size /= 4u;
        pixel_size_bytes = pixel_storage_size((PixelStorage)_pixel_storage, uint3(4u, 4u, 1u));
        chunk_resolution /= 4;
    } else {
        pixel_size_bytes = pixel_storage_size((PixelStorage)_pixel_storage, uint3(1u));
    }
    raw_size_bytes = pixel_size_bytes * chunk_resolution;
    auto block_size_bytes = raw_size_bytes * chunk_resolution;
    auto block_count = (size + chunk_resolution - 1u) / chunk_resolution;
    auto get_src_offset = [&](uint2 coord) {
        return (coord.y * size.x + coord.x) * pixel_size_bytes;
    };
    auto get_dst_offset = [&](uint2 coord) {
        auto block_index = coord / chunk_resolution;
        auto block_offset = block_index.x + block_index.y * block_count.x;
        auto local_coord = (coord & (chunk_resolution - 1));
        return block_offset * block_size_bytes + pixel_size_bytes * (local_coord.x + local_coord.y * chunk_resolution);
    };

    for (auto block_x : vstd::range(block_count.x))
        for (auto block_y : vstd::range(block_count.y)) {
            for (auto raw : vstd::range(chunk_resolution)) {
                uint2 coord = uint2(block_x, block_y) * chunk_resolution + uint2(0, raw);
                const auto src_offset = get_src_offset(coord);
                const auto dst_offset = get_dst_offset(coord);
                std::memcpy(dst.data() + dst_offset, src.data() + src_offset, raw_size_bytes);
            }
        }
}
bool TextureResource::pack_to_tile() {
    if (_is_vt) return {};
    auto host_data_ = host_data();
    if (!host_data_ || host_data_->empty()) return {};
    luisa::vector<std::byte> data;
    data.push_back_uninitialized(host_data_->size());
    auto size = _size;
    uint64_t offset = 0;
    for (auto i : vstd::range(_mip_level)) {
        auto level_size = pixel_storage_size((PixelStorage)_pixel_storage, make_uint3(size, 1u));
        _pack_to_tile_level(
            i,
            luisa::span{*host_data_}.subspan(offset, level_size),
            luisa::span{data}.subspan(offset, level_size));
        size >>= 1u;
        offset += level_size;
    }
    *host_data_ = std::move(data);
    _is_vt = true;
    return true;
}
void TextureLoader::finish_task() {
    {
        std::lock_guard lck{_mtx};
        if (!_cmdlist.empty()) {
            RenderDevice::instance().lc_main_stream() << _cmdlist.commit() << _event.signal(_finished_fence++);
        }
        while (auto v = _after_device_task.pop()) {
            while (!_event.is_completed(v->second)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            luisa::fiber::schedule([counter = _counter, func = std::move(v->first)]() {
                func();
                counter.done();
            });
        }
    }
    if (_finished_fence > 1)
        while (!_event.is_completed(_finished_fence - 1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    _counter.wait();
}

}// namespace rbc::world