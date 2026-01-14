#include <rbc_world/resources/texture.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_world/type_register.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/importers/texture_importer_stb.h>
#include <stb/stb_image.h>
#include <tinytiffreader.h>
#include <tinyexr.h>
#include <rbc_world/texture_loader.h>
#include <luisa/core/binary_io.h>

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

void TextureLoader::process_texture(RC<TextureResource> const &tex, uint mip_level, bool to_vt) {

    auto render_device = RenderDevice::instance_ptr();
    if (mip_level > 1 || to_vt) {
        LUISA_ASSERT(render_device, "Render device must be initialized.");
        std::lock_guard lck{_mtx};
        if (!_pack_tex) {
            _pack_tex.create(render_device->lc_device());
        }
        if (mip_level > 1 && !_event) {
            _event = render_device->lc_device().create_timeline_event();
        }
    }
    uint chunk_size = TexStreamManager::chunk_resolution;
    if (is_block_compressed((PixelStorage)tex->pixel_storage())) {
        chunk_size /= 4;
    }

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
        tex->install();
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