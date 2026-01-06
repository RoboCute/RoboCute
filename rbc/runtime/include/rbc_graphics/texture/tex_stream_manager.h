#pragma once
#include <rbc_config.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/sparse_image.h>
#include <luisa/runtime/sparse_heap.h>
#include <luisa/vstl/lockfree_array_queue.h>
#include <rbc_io/io_service.h>
#include <rbc_graphics/bindless_allocator.h>
#include <rbc_graphics/bindless_manager.h>
#include <rbc_graphics/buffer_allocator.h>
#include <rbc_graphics/buffer_uploader.h>
#include <rbc_graphics/host_buffer_manager.h>
#include <rbc_core/shared_atomic_mutex.h>
#include "luisa/core/logging.h"
#include "tile_streamer.h"
#include <luisa/runtime/sparse_command_list.h>

namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct RBC_RUNTIME_API TexStreamManager {
public:
    // WARNING:+  This function will be called currently
    using RuntimeVTCallback = vstd::function<void(
        ImageView<float> img,
        luisa::span<const uint2> update_tiles)>;
    using FilePath = std::pair<luisa::string, uint64_t>;
    using TexPath = vstd::variant<FilePath, RuntimeVTCallback>;
    struct RBC_RUNTIME_API SparseHeap {
        TexStreamManager *self{nullptr};
        SparseTextureHeap heap;
        size_t size;
        SparseHeap() {}
        SparseHeap(
            TexStreamManager *self,
            size_t size);
        SparseHeap(SparseHeap const &) = delete;
        SparseHeap(SparseHeap &&rhs);
        ~SparseHeap();
        SparseHeap &operator=(SparseHeap &&rhs) {
            this->~SparseHeap();
            new (std::launder(this)) SparseHeap(std::move(rhs));
            return *this;
        }
        operator bool() const {
            return heap.operator bool();
        }
    };
    friend struct SparseHeap;
    struct Coord {
        std::array<uint, 2> tile_idx;
        uint level;
        Coord(
            std::array<uint, 2> tile_idx,
            uint level)
            : tile_idx(tile_idx), level(level) {
        }
        Coord(
            uint2 tile_idx,
            uint level)
            : tile_idx({tile_idx.x, tile_idx.y}), level(level) {
        }
        bool operator==(Coord const &rhs) const {
            return memcmp(this, &rhs, sizeof(Coord)) == 0;
        }
    };
    struct CoordHash {
        size_t operator()(Coord const &c) const {
            return luisa::hash64(&c, sizeof(Coord), luisa::hash64_default_seed);
        }
    };
    struct TexIndex : public luisa::enable_shared_from_this<TexIndex>, RBCStruct {
        BufferAllocator::Node node;
        SparseImage<float> img;
        TexPath path;
        IOFile tex_file;
        vector<SparseHeap> heaps;
        TileStreamer streamer;
        uint bindless_idx;
        uint loaded_countdown;
        uint vector_idx{std::numeric_limits<uint>::max()};
        luisa::move_only_function<void()> init_callback;
        SparseHeap &get_heap(uint2 tile_idx, uint level) {
            uint2 res = streamer.resolution();
            uint sz{0};
            for (auto i : vstd::range(level)) {
                LUISA_ASSERT(res.x >= 1 && res.y >= 1, "Resolution must be larger than 0.");
                sz += res.x * res.y;
                res >>= 1u;
            }
            sz += tile_idx.y * res.x + tile_idx.x;
            return heaps[sz];
        }
        size_t get_byte_offset(uint2 tile_idx, uint level);
        template<typename... Args>
            requires(luisa::is_constructible_v<TileStreamer, Args && ...>)
        TexIndex(Args &&...args)
            : streamer(std::forward<Args>(args)...) {
        }
    };
    enum struct Op : uint32_t {
        Load,
        UnLoad
    };
    template<template<typename> typename Ptr>
    struct LoadCommand {
        Ptr<TexIndex> tex_idx;
        vector<std::pair<uint2, uint>> tiles;
        LoadCommand(weak_ptr<TexIndex> const &t)
            : tex_idx(t) {};
    };
    struct SetLevelCommand {
        uint offset;
        uint value;
    };
    struct SetRange {
        uint offset;
        uint size;
    };
private:
    Device &_device;
    Stream &_async_stream;
    IOService &_io_service;
    BindlessManager &_bdls_mng;
    BufferUploader _uploader;
    Buffer<uint16_t> _min_level_buffer;
    Buffer<uint> _chunk_offset_buffer;
    BufferAllocator _level_buffer;
    std::mutex _async_mtx;
    vstd::HashMap<uint64, shared_ptr<TexIndex>> _loaded_texs;
    luisa::spin_mutex _dispose_map_mtx;
    vstd::HashMap<uint, SetRange> _dispose_map;
    Shader1D<Buffer<uint16_t>, uint> const *set_shader16;
    Shader1D<Buffer<uint16_t>, Buffer<uint2>, Buffer<uint>, uint> const *set_min_level_shader;
    Shader1D<Buffer<uint>, uint> const *set_shader;
    Shader1D<Buffer<uint>, uint, uint> const *clear_shader;
    // used in before_rendering
    luisa::vector<uint2> dispose_offset_cache;
    luisa::vector<uint3> dispose_dispatch_cache;
    uint _countdown{(1u << 28u) - 2u};
    ////////////// callback thread	struct
    struct FrameResource {
        vector<SetLevelCommand> set_level_cmds;
        vector<LoadCommand<weak_ptr>> unload_cmds;
        vstd::vector<shared_ptr<TexIndex>> depended_texs;
    };
    vstd::LockFreeArrayQueue<FrameResource> _frame_res;
    vstd::LockFreeArrayQueue<vstd::vector<uint>> _frame_datas;
    struct UInt3Equal {
        bool operator()(uint3 const &a, uint3 const &b) const {
            return all(a == b);
        }
    };
    vstd::vector<TexIndex *> _remove_list;
    struct UnmapCmd {
        vstd::unordered_set<uint3, luisa::hash<uint3>, UInt3Equal> map;
        luisa::spin_mutex _mtx;
    };
    vstd::HashMap<TexIndex *, UnmapCmd> _unmap_lists;
    uint inqueue_frame{0};
    vector<TexIndex *> _tex_indices;

    ////////////// callback thread
    size_t _readback_size{};
    mutable std::atomic_size_t _allocated_size{0};
    // Make sure readback processor serial with load & unload
    luisa::spin_mutex _uploader_mtx;
    size_t _allocate_size_limits;
    size_t _memoryless_threshold;
    const uint8_t _lru_frame;
    const uint8_t _lru_frame_memoryless;
    IOCommandList _process_readback(vstd::span<uint const> readback, SparseCommandList &map_cmdlist);
    luisa::compute::TimelineEvent _main_stream_event;

    struct CopyStreamCallback {
        TexStreamManager *self;
        Stream &stream;
        IOCommandList io_cmdlist;
        SparseCommandList sparse_cmdlist;
        TimelineEvent *event;
        uint64_t fence;
        luisa::vector<SparseTextureHeap> disposed_heap;
        void operator()();
    };
    vstd::LockFreeArrayQueue<CopyStreamCallback> _copy_stream_callbacks;
    std::thread _copy_stream_thd;
    std::atomic_bool _enabled{true};
    std::mutex _copy_stream_mtx;
    std::condition_variable _copy_stream_cv;
    CommandList async_cmdlist;
    uint64_t signalled_fence = 0;
    std::atomic_uint64_t last_fence = 0;
    void _async_logic();
    luisa::fiber::event _async_load_evt;
    uint64_t _last_io_fence = 0;

public:
    [[nodiscard]] auto const &min_level_buffer() const { return _min_level_buffer; }
    [[nodiscard]] auto const &offset_buffer() const { return _chunk_offset_buffer; }
    [[nodiscard]] auto const &level_buffer() const { return _level_buffer.buffer(); }
    [[nodiscard]] auto countdown() const { return _countdown; }
    [[nodiscard]] auto &io_service() { return _io_service; }

    static const uint chunk_resolution;
    static TexStreamManager *instance();
    struct LoadResult {
        SparseImage<float> const *img_ptr;
        TexIndex *tex_idx;
    };
    TexStreamManager(
        Device &device,
        Stream &async_stream,
        IOService &io_service,
        CommandList &cmdlist,
        BindlessManager &bdls_mng,
        // unload texture's one level after <lru_frame> frame
        uint8_t lru_frame = 16,
        uint8_t lru_frame_memoryless = 4,
        size_t memoryless_threshold = 1024ull * 1024ull * 8ull,
        // default 16 GB
        size_t memory_limit = 16ull * 1024ull * 1024ull * 1024ull);
    ~TexStreamManager();
    LoadResult load_sparse_img(
        SparseImage<float> &&img,
        TexPath &&path,
        uint bindless_mng_index,
        DisposeQueue &disp_queue,
        CommandList &cmdlist,
        luisa::move_only_function<void()> &&init_callback);
    void unload_sparse_img(
        uint64 img_uid,
        DisposeQueue &disp_queue);
    void before_rendering(
        Stream &main_stream,
        HostBufferManager &temp_buffer,
        CommandList &cmdlist);
    void force_sync();
};
}// namespace rbc