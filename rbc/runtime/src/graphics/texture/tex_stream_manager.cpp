#include <rbc_graphics/texture/tex_stream_manager.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/logging.h>
#include <luisa/core/stl/algorithm.h>
#include <rbc_io/io_service.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <rbc_core/binary_file_writer.h>
namespace rbc::detail {
template<typename Load, typename UnLoad, typename Check>
    requires(
        std::is_invocable_v<Load, uint2, uint> &&
        std::is_invocable_v<UnLoad, uint2, uint> &&
        std::is_invocable_r_v<bool, Check, uint2, uint>)
class TexStreamCallback : public TileStreamer::LoadCallback {
    Load &&_load;
    UnLoad &&_unload;
    Check &&_check;

public:
    TexStreamCallback(Load &&load, UnLoad &&unload, Check &&check)
        : _load(std::forward<Load>(load)), _unload(std::forward<UnLoad>(unload)), _check(std::forward<Check>(check)) {
    }
    bool can_load(uint2 tile_index, uint level) override {
        return _check(tile_index, level);
    }

    void load_tile(uint2 tile_index, uint level) override {
        _load(tile_index, level);
    }
    void unload_tile(uint2 tile_index, uint level) override {
        _unload(tile_index, level);
    }
};
static TexStreamManager *_tex_mng_inst = nullptr;
}// namespace rbc::detail
namespace rbc {
TexStreamManager *TexStreamManager::instance() {
    return detail::_tex_mng_inst;
}
const uint TexStreamManager::chunk_resolution = 256;
TexStreamManager::TexStreamManager(
    Device &device,
    Stream &async_stream,
    IOService &io_service,
    CommandList &cmdlist,
    BindlessManager &bdls_mng,
    uint8_t lru_frame,
    uint8_t lru_frame_memoryless,
    size_t memoryless_threshold,
    size_t memory_limit)
    : _device(device), _async_stream(async_stream), _io_service(io_service), _bdls_mng(bdls_mng), _level_buffer(device, 65536 * sizeof(uint)), _allocate_size_limits(memory_limit), _memoryless_threshold(memoryless_threshold), _lru_frame(lru_frame), _lru_frame_memoryless(lru_frame_memoryless), _copy_stream_thd([this]() {
          RenderDevice::set_rendering_thread(false);
          auto clear_all = vstd::scope_exit([&]() {
              while (auto p = _copy_stream_callbacks.pop()) {
                  p->sparse_cmdlist.clear();
              }
          });
          while (_enabled.load()) {
              while (auto p = _copy_stream_callbacks.pop()) {
                  {
                      std::unique_lock lck{_copy_stream_mtx};
                      while (p->fence > signalled_fence) {
                          _copy_stream_cv.wait(lck);
                          if (!_enabled.load()) {
                              p->sparse_cmdlist.clear();
                              return;
                          }
                      }
                  }
                  (*p)();
              }
              std::unique_lock lck{_copy_stream_mtx};
              _copy_stream_cv.wait(lck);
          }
      }),
      _async_load_evt(luisa::fiber::event::Mode::Manual, true) {
    if (!detail::_tex_mng_inst)
        detail::_tex_mng_inst = this;
    if (lru_frame < 3 || lru_frame_memoryless < 3) [[unlikely]] {
        LUISA_ERROR("LRU frame can not be less than 3 frame, this may cause flicking or false unload.");
    }
    luisa::fiber::counter counter;
    _uploader.load_shader(counter);
    _frame_res.reserve(64);
    _min_level_buffer = device.create_buffer<uint>(65536);
    _chunk_offset_buffer = device.create_buffer<uint>(262144u);
    ShaderManager::instance()->load("texture_process/tex_mng_clear.bin", clear_shader);
    ShaderManager::instance()->load("path_tracer/clear_buffer.bin", set_shader);
    cmdlist << (*set_shader)(_min_level_buffer, 16u).dispatch(_min_level_buffer.size())
            << (*set_shader)(_chunk_offset_buffer, std::numeric_limits<uint>::max()).dispatch(_chunk_offset_buffer.size());
    bdls_mng.alloc().set_reserved_buffer(heap_indices::chunk_offset_buffer_idx, _chunk_offset_buffer);
    bdls_mng.alloc().set_reserved_buffer(heap_indices::min_level_buffer_idx, _min_level_buffer);
    _main_stream_event = device.create_timeline_event();
    counter.wait();
}

size_t TexStreamManager::TexIndex::get_byte_offset(uint2 tile_idx, uint level) {
    size_t size_offset = 0;
    size_t tile_size = pixel_storage_size(img.storage(), uint3(chunk_resolution, chunk_resolution, 1));
    uint2 res = streamer.resolution();
    for (auto i : vstd::range(level)) {
        LUISA_ASSERT(res.x >= 1 && res.y >= 1, "Resolution must be larger than 0.");
        size_offset += res.x * res.y * tile_size;
        res >>= 1u;
    }
    size_offset += (tile_idx.x + tile_idx.y * res.x) * tile_size;
    return size_offset;
}
void TexStreamManager::CopyStreamCallback::operator()() {
    stream << event->wait(fence);
    if (!sparse_cmdlist.empty()) {
        stream << sparse_cmdlist.commit();
    }
    if (!io_cmdlist.empty() || !disposed_heap.empty()) {
        stream << [self = this->self, io_cmdlist = std::move(io_cmdlist), disposed_heap = std::move(disposed_heap)]() mutable {
            if (!disposed_heap.empty())
                disposed_heap.clear();
            if (!io_cmdlist.empty())
                self->_last_io_fence = self->io_service().execute(std::move(io_cmdlist));
        };
    }
}
void TexStreamManager::_async_logic() {
    CommandList &cmdlist = async_cmdlist;
    luisa::vector<SparseTextureHeap> _disposed_heap;
    auto remove_list = [&]() {
        return std::move(_remove_list);
    }();
    for (auto &i : remove_list) {
        _unmap_lists.remove(i);
    }

    if (auto res = _frame_res.pop()) {
        inqueue_frame--;
        auto evt = luisa::fiber::async(
            [&]() {
                std::lock_guard lck{_uploader_mtx};
                if (res->set_level_cmds.empty()) return;
                auto &copy_cmd = _uploader._get_copy_cmd(_min_level_buffer.view());
                for (auto &i : res->set_level_cmds) {
                    copy_cmd.indices.emplace_back(i.offset);
                    auto size = copy_cmd.datas.size();
                    copy_cmd.datas.push_back_uninitialized(sizeof(uint));
                    auto ptr = reinterpret_cast<uint *>(copy_cmd.datas.data() + size);
                    *ptr = i.value;
                }
            });
        for (auto &i : res->depended_texs) {
            if (i->init_callback)
                i->init_callback();
        }
        // for (auto&& tex : res->load_cmds) {
        // 	auto& tex_idx = tex.tex_idx;
        // 	for (auto&& tile : tex.tiles) {

        // 	}
        // }
        std::mutex _unload_mtx;
        luisa::fiber::parallel(
            res->unload_cmds.size(),
            [&](size_t i) {
                auto &tile = res->unload_cmds[i];
                auto tex = tile.tex_idx.lock();
                if (!tex) {
                    return;
                }
                _unload_mtx.lock();
                auto iter = _unmap_lists.emplace(tex.get());
                auto &v = iter.value();
                _unload_mtx.unlock();
                luisa::fiber::parallel(
                    tile.tiles.size(),
                    [&](size_t idx) {
                        auto &i = tile.tiles[idx];
                        tex->streamer.sample_node(i.first, i.second).processing_count--;
                        v._mtx.lock();
                        v.map.emplace(make_uint3(i.first, i.second));
                        v._mtx.unlock();
                    },
                    128);
            });
        evt.wait();
    }
    SparseCommandList sparse_cmdlist;
    IOCommandList io_cmdlist;
    if (auto readback_data = _frame_datas.pop()) {
        io_cmdlist = _process_readback(*readback_data, sparse_cmdlist);
    }
    {
        vector<TexIndex *> remove_unmap;
        vector<uint3> remove_lists;
        for (auto &i : _unmap_lists) {
            remove_lists.clear();
            auto tex_pixel_size = pixel_storage_size(i.first->img.storage(), uint3(chunk_resolution, chunk_resolution, 1));
            auto tex_is_compress = is_block_compressed(i.first->img.storage());

            for (auto &tile : i.second.map) {
                auto &node = i.first->streamer.sample_node(tile.xy(), tile.z);
                if (node.processing_count != 0 || node.ref_count != 0) continue;
                auto sparse_tile_size = uint2(chunk_resolution) / i.first->img.tile_size();
                auto &heap = i.first->get_heap(tile.xy(), tile.z);
                if (heap.heap) {
                    sparse_cmdlist << i.first->img.unmap_tile(
                        tile.xy() * sparse_tile_size,
                        sparse_tile_size,
                        tile.z);
                    remove_lists.emplace_back(tile);
                    _disposed_heap.emplace_back(std::move(heap.heap));
                }
            }
            if (remove_lists.empty()) continue;
            if (remove_lists.size() == i.second.map.size()) {
                remove_unmap.emplace_back(i.first);
            } else {
                for (auto &idx : remove_lists) {
                    i.second.map.erase(idx);
                }
            }
        }
        for (auto &i : remove_unmap) {
            _unmap_lists.remove(i);
        }
    }
    if (!io_cmdlist.empty() || !sparse_cmdlist.empty() || !_disposed_heap.empty()) {
        _copy_stream_callbacks.push(
            this,
            _async_stream,
            std::move(io_cmdlist),
            std::move(sparse_cmdlist),
            &_main_stream_event,
            last_fence.load(),
            std::move(_disposed_heap));
    }
    if (inqueue_frame >= 2) {
        return;
    }
    inqueue_frame++;
    ///////////////// Process async commands
    if (_readback_size > 0) {
        vector<uint> readback;
        readback.push_back_uninitialized(_readback_size / sizeof(uint));
        cmdlist << _level_buffer.buffer().view(0, readback.size()).copy_to(readback.data());
        cmdlist.add_callback([this, readback = std::move(readback)]() mutable {
            _frame_datas.push(std::move(readback));
        });
    }
    _countdown--;
    if (_countdown == 0) {
        _countdown = (1u << 28u) - 1u;
        auto const &bf = _level_buffer.buffer();
        cmdlist << (*clear_shader)(
                       bf,
                       _countdown << 4u,
                       ~15u)
                       .dispatch(bf.size());
        for (auto &&i : _loaded_texs) {
            i.second->loaded_countdown = _countdown;
        }
    }
}

TexStreamManager::~TexStreamManager() {
    if (detail::_tex_mng_inst == this)
        detail::_tex_mng_inst = nullptr;
    _copy_stream_mtx.lock();
    _enabled = false;
    signalled_fence = last_fence.load();
    _copy_stream_mtx.unlock();
    _copy_stream_cv.notify_one();
    _copy_stream_thd.join();
    _async_load_evt.wait();
    async_cmdlist.clear();
    for (auto &i : _loaded_texs) {
        i.second->heaps.clear();
        _level_buffer.free(i.second->node);
    }
    _async_stream << _main_stream_event.signal(signalled_fence + 1);
    _async_stream.synchronize();
    if (_last_io_fence > 0) {
        _io_service.synchronize(_last_io_fence);
    }
    _main_stream_event.synchronize(signalled_fence + 1);
}

auto TexStreamManager::load_sparse_img(
    SparseImage<float> &&img,
    TexPath &&path,
    uint bindless_idx,
    DisposeQueue &disp_queue,
    CommandList &cmdlist,
    luisa::move_only_function<void()> &&init_callback) -> LoadResult {
    _async_load_evt.wait();
    std::lock_guard lck{_async_mtx};
    PixelStorage storage = img.storage();
    uint2 resolution = img.size();
    uint mip = img.mip_levels();
    LUISA_ASSERT(any(resolution >> (mip - 1) >= chunk_resolution), "Minimum level mip must be less than chunk_resolution {}", chunk_resolution);
    uint2 tile_count = (resolution + (chunk_resolution - 1u)) / chunk_resolution;
    auto iter = _loaded_texs.emplace(img.uid(), vstd::lazy_eval([&]() {
                                         return luisa::make_shared<TexIndex>(tile_count, img.mip_levels(), true);
                                     }));
    auto &v = iter.value();
    v->loaded_countdown = _countdown;
    v->bindless_idx = bindless_idx;
    _dispose_map_mtx.lock();
    _dispose_map.remove(bindless_idx);
    _dispose_map_mtx.unlock();
    v->img = std::move(img);
    v->path = std::move(path);
    v->heaps.resize(v->streamer.count());
    v->init_callback = std::move(init_callback);
    {
        std::lock_guard lck{_uploader_mtx};
        auto callback = [&](Buffer<uint> const &old_buffer, Buffer<uint> const &new_buffer) {
            _uploader.swap_buffer(old_buffer, new_buffer);
            auto new_min_level_buffer = _device.create_buffer<uint>(new_buffer.size());
            auto rest_view = new_min_level_buffer.view(_min_level_buffer.size(), new_min_level_buffer.size() - _min_level_buffer.size());
            cmdlist << new_min_level_buffer.view(0, _min_level_buffer.size()).copy_from(_min_level_buffer)
                    << (*set_shader)(rest_view, 16u).dispatch(rest_view.size());
            _uploader.swap_buffer(_min_level_buffer, new_min_level_buffer);
            disp_queue.dispose_after_queue(std::move(_min_level_buffer));
            _min_level_buffer = std::move(new_min_level_buffer);
            _bdls_mng.alloc().set_reserved_buffer(heap_indices::min_level_buffer_idx, _min_level_buffer);
        };
        v->node = _level_buffer.allocate(
            _device,
            cmdlist,
            disp_queue,
            tile_count.x * tile_count.y * sizeof(uint),
            callback,
            BufferAllocator::AllocateType::BestFit);
        auto host_ptr = _uploader.emplace_copy_cmd(v->node.view<uint>(_level_buffer));
        auto offset_ptr = _uploader.emplace_copy_cmd(_chunk_offset_buffer.view(bindless_idx, 1));
        _readback_size = std::max<size_t>(_readback_size, v->node.offset_bytes() + v->node.size_bytes());
        const uint value = (_countdown << 4u) | 15u;
        for (auto end_ptr = host_ptr + tile_count.x * tile_count.y; host_ptr != end_ptr; ++host_ptr) {
            *host_ptr = value;
        }
        // level-buffer's pos
        *offset_ptr = v->node.offset_bytes() / sizeof(uint);
    }
    {
        auto add_idx = v.get();
        add_idx->vector_idx = _tex_indices.size();
        _tex_indices.emplace_back(add_idx);
    }
    return LoadResult{
        &v->img,
        v.get()};
}

void TexStreamManager::unload_sparse_img(
    uint64 img_uid,
    DisposeQueue &disp_queue) {
    _async_load_evt.wait();
    std::lock_guard lck{_async_mtx};
    auto iter = _loaded_texs.find(img_uid);
    LUISA_ASSERT(iter, "Trying to destroy non-exists tex.");
    auto &v = iter.value();
    _dispose_map_mtx.lock();
    _dispose_map.emplace(v->bindless_idx);
    _dispose_map_mtx.unlock();
    _bdls_mng.alloc().deallocate_tex2d(v->bindless_idx);
    _level_buffer.free(v->node);
    if (v->vector_idx != std::numeric_limits<uint>::max()) [[likely]] {
        auto vec_idx = v->vector_idx;
        if (vec_idx != (_tex_indices.size() - 1)) {
            auto &v = _tex_indices[vec_idx];
            v = _tex_indices.back();
            v->vector_idx = vec_idx;
        }
        _tex_indices.pop_back();
    }
    _remove_list.emplace_back(v.get());
    disp_queue.dispose_after_queue(std::move(v));
    _loaded_texs.remove(iter);
}

IOCommandList TexStreamManager::_process_readback(vstd::span<uint const> readback, SparseCommandList &map_cmdlist) {
    FrameResource frame_res;
    IOCommandList io_cmdlist;
    luisa::spin_mutex depended_tex_mtx, io_mtx, map_mtx, frame_res_mtx;

    vstd::vector<shared_ptr<TexIndex>> depended_texs;
    std::atomic_size_t predict_allocated_size = 0;
    std::atomic_size_t chunk_size_bytes = 0;
    std::atomic_size_t already_allocated = _allocated_size.load();
    auto size = _tex_indices.size();
    luisa::fiber::parallel(size, [&](size_t idx) {
        if (idx >= _tex_indices.size()) return;
        auto tex_idx_ptr = _tex_indices[idx];
        auto &tex_idx = *tex_idx_ptr;
        size_t chunk_allocated_size = 0;

        vstd::unordered_map<Coord, bool, CoordHash> load_cmds;
        auto load = [&](uint2 tile_idx, uint level) {
            predict_allocated_size += chunk_size_bytes;
            load_cmds.force_emplace(Coord(tile_idx, level), true);
        };
        auto unload = [&](uint2 tile_idx, uint level) {
            load_cmds.force_emplace(Coord(tile_idx, level), false);
        };
        auto check = [&](uint2 tile_idx, uint level) {
            chunk_allocated_size += chunk_size_bytes;
            return already_allocated + chunk_allocated_size < _allocate_size_limits;
        };
        rbc::detail::TexStreamCallback<decltype(load) &, decltype(unload) &, decltype(check) &> callback{load, unload, check};
        std::array<vstd::vector<uint2>, 14> runtime_tiles;

        std::atomic_bool depended = false;
        auto node_offset = tex_idx.node.offset_bytes() / sizeof(uint);
        uint const *rb_ptr = readback.data() + node_offset;
        uint chunk_count = tex_idx.node.size_bytes() / sizeof(uint);
        chunk_size_bytes = pixel_storage_size(tex_idx.img.storage(), uint3(chunk_resolution, chunk_resolution, 1));
        uint2 tile_size = uint2(chunk_resolution) / tex_idx.img.tile_size();
        size_t unload_cmd_idx = std::numeric_limits<size_t>::max();
        auto last_level = tex_idx.img.mip_levels() - 1;
        uint2 tile_count = (tex_idx.img.size() + (chunk_resolution - 1u)) / chunk_resolution;
        uint lru_frame;
        if (already_allocated >= _allocate_size_limits - _memoryless_threshold) {
            lru_frame = _lru_frame_memoryless;
        } else {
            lru_frame = _lru_frame;
        }
        for (auto chunk_idx : vstd::range(chunk_count)) {
            if ((size_t)rb_ptr < 99999) {
                LUISA_WARNING("{}", (size_t)readback.data());
            }
            auto rb_value = rb_ptr[chunk_idx];
            uint last_updated_countdown = rb_value >> 4u;
            uint dst_level = rb_value & 15u;
            // this readback is earlier than SparseImage's loaded frame
            if (last_updated_countdown > tex_idx.loaded_countdown) {
                continue;
            }
            uint frame_diff = last_updated_countdown - _countdown;
            uint2 tile_idx;
            tile_idx.y = chunk_idx / tile_count.x;
            tile_idx.x = chunk_idx - (tile_idx.y * tile_count.x);
            auto &cur_lru_frame = tex_idx.streamer.frame_lru(tile_idx);
            if (frame_diff <= _lru_frame_memoryless || cur_lru_frame == 0) {
                cur_lru_frame = lru_frame;
            } else {
                cur_lru_frame = std::min<uint8_t>(cur_lru_frame, lru_frame);
            }
            chunk_allocated_size = predict_allocated_size;
            dst_level += frame_diff / cur_lru_frame;
            dst_level = std::min<uint>(dst_level, last_level);

            uint current_level = tex_idx.streamer.tile_level(tile_idx);
            if (current_level != dst_level) {
                uint can_reach_level = tex_idx.streamer.can_load(callback, tile_idx, dst_level);
                if (current_level != can_reach_level) {
                    tex_idx.streamer.load_tile(callback, tile_idx, can_reach_level);
                    SetLevelCommand set_level_cmd{
                        .offset = static_cast<uint>(node_offset + chunk_idx),
                        .value = can_reach_level};
                    std::lock_guard lck{frame_res_mtx};
                    frame_res.set_level_cmds.emplace_back(std::move(set_level_cmd));
                }
            }
        }
        bool runtime_dirty = false;
        tex_idx.path.visit([&]<typename T>(T const &t) {
            auto tex_pixel_size = pixel_storage_size(tex_idx.img.storage(), uint3(chunk_resolution, chunk_resolution, 1));
            const bool is_runtime_vt = std::is_same_v<T, RuntimeVTCallback>;
            auto iter = load_cmds.begin();
            luisa::spin_mutex load_cmd_ite_mtx;
            luisa::fiber::parallel(
                load_cmds.size(),
                [&](size_t i) {
                    load_cmd_ite_mtx.lock();
                    auto &u3 = *iter;
                    iter++;
                    load_cmd_ite_mtx.unlock();
                    auto tile_idx = uint2{u3.first.tile_idx[0], u3.first.tile_idx[1]};
                    auto level = u3.first.level;
                    if (u3.second) {
                        auto &heap = tex_idx.get_heap(tile_idx, level);
                        tex_idx.streamer.sample_node(tile_idx, level).processing_count++;
                        if (!heap) {
                            heap = SparseHeap(
                                this,
                                tex_pixel_size);
                        }
                        if (!depended) [[unlikely]] {
                            auto shared_ptr = tex_idx.shared_from_this();
                            std::lock_guard lck{depended_tex_mtx};
                            if (!depended) [[unlikely]] {
                                depended_texs.emplace_back(std::move(shared_ptr));
                                depended = true;
                            }
                        }
                        {
                            auto map_cmd = tex_idx.img.map_tile(tile_idx * tile_size, tile_size, level, heap.heap);
                            std::lock_guard lck{map_mtx};
                            map_cmdlist << std::move(map_cmd);
                        }
                        if constexpr (is_runtime_vt) {
                            runtime_dirty = true;
                            runtime_tiles[level].emplace_back(tile_idx);
                        } else {
                            if (!tex_idx.tex_file) [[unlikely]] {
                                tex_idx.tex_file = IOFile{t.first};
                            }
                            IOCommand io_cmd{
                                tex_idx.tex_file,
                                t.second + tex_idx.get_byte_offset(tile_idx, level),
                                IOTextureSubView{
                                    tex_idx.img.view(level),
                                    tile_idx * uint2(chunk_resolution),
                                    uint2(chunk_resolution)}};
                            std::lock_guard lck{io_mtx};
                            io_cmdlist << std::move(io_cmd);
                        }
                    } else {
                        std::lock_guard lck{frame_res_mtx};
                        if (unload_cmd_idx == std::numeric_limits<size_t>::max()) {
                            unload_cmd_idx = frame_res.unload_cmds.size();
                            frame_res.unload_cmds.emplace_back(tex_idx.weak_from_this());
                        }
                        frame_res.unload_cmds[unload_cmd_idx].tiles.emplace_back(tile_idx, level);
                    }
                },
                128);
            if constexpr (is_runtime_vt) {
                if (runtime_dirty) {
                    for (auto idx : vstd::range(tex_idx.img.mip_levels())) {
                        auto &tiles = runtime_tiles[idx];
                        if (tiles.empty()) continue;
                        t(tex_idx.img.view(idx), tiles);
                    }
                }
            }
        });
    });
    io_cmdlist.add_callback([this, frame_res = std::move(frame_res), depended_texs = std::move(depended_texs)]() mutable {
        frame_res.depended_texs = std::move(depended_texs);
        _frame_res.push(std::move(frame_res));
    });
    return io_cmdlist;
}
TexStreamManager::SparseHeap::SparseHeap(
    TexStreamManager *self,
    size_t size)
    : self(self), heap(self->_device.allocate_sparse_texture_heap(size)), size(size) {
    self->_allocated_size += size;
}
TexStreamManager::SparseHeap::SparseHeap(SparseHeap &&rhs)
    : self(rhs.self), heap(std::move(rhs.heap)), size(rhs.size) {
    rhs.self = nullptr;
}
TexStreamManager::SparseHeap::~SparseHeap() {
    if (self) {
        self->_allocated_size -= size;
    }
}

void TexStreamManager::force_sync() {
    _async_load_evt.wait();
    while (_copy_stream_callbacks.length() > 0) {
        _copy_stream_cv.notify_one();
        std::this_thread::yield();
    }
    _async_stream << synchronize();
    if (_last_io_fence > 0) {
        _io_service.synchronize(_last_io_fence);
    }
}

void TexStreamManager::before_rendering(
    Stream &main_stream,
    HostBufferManager &temp_buffer,
    CommandList &cmdlist) {
    if (!_async_load_evt.is_signalled()) {
        return;
    }
    cmdlist.add_range(std::move(async_cmdlist));
    {
        std::lock_guard lck{_uploader_mtx};
        {
            std::lock_guard lck1{_dispose_map_mtx};
            if (!_dispose_map.empty()) {
                auto &copy_cmd = _uploader._get_copy_cmd(_chunk_offset_buffer.view());
                for (auto &i : _dispose_map) {
                    copy_cmd.indices.emplace_back(i);
                    auto size = copy_cmd.datas.size();
                    copy_cmd.datas.push_back_uninitialized(sizeof(uint));
                    auto ptr = reinterpret_cast<uint *>(copy_cmd.datas.data() + size);
                    *ptr = std::numeric_limits<uint>::max();
                }
                _dispose_map.clear();
            }
        }
        _uploader.commit(cmdlist, temp_buffer);
    }
    auto fence = ++last_fence;

    if (fence > 0)
        main_stream << Event::Signal{
            _main_stream_event.handle(),
            fence};
    _copy_stream_mtx.lock();
    signalled_fence = fence;
    _copy_stream_mtx.unlock();
    _copy_stream_cv.notify_one();
    _async_load_evt.clear();
    luisa::fiber::schedule(
        [this]() {
            {
                std::lock_guard lck{_async_mtx};
                _async_logic();
            }
            _async_load_evt.signal();
        });
}
}// namespace rbc