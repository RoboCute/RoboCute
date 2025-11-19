#pragma once
#include <rbc_config.h>
#include <luisa/core/mathematics.h>
#include <luisa/vstl/common.h>
namespace rbc
{

using namespace luisa;
class TileNodes
{
public:
    struct Node {
        uint32_t ref_count;
        std::atomic_uint32_t processing_count;
        Node() = default;
        Node(Node const&) = delete;
        Node(Node&& rhs)
            : ref_count(rhs.ref_count)
            , processing_count(rhs.processing_count.load())
        {
        }
        ~Node() = default;
    };

private:
    vstd::vector<Node> _nodes;
    uint2 _resolution;
    uint _mip;
    size_t _idx(uint2 tile_index, uint level);
    size_t _offset(uint level);

public:
    [[nodiscard]] auto count() const { return _nodes.size(); }
    TileNodes(uint2 resolution, uint mip);
    ~TileNodes();
    Node& sample_node(uint2 tile_index, uint level);
    bool try_load_chunk(uint2 tile_index, uint level);
    bool try_unload_chunk(uint2 tile_index, uint level);
    bool require_load(uint2 tile_index, uint level);
    TileNodes(TileNodes const&) = delete;
    TileNodes(TileNodes&&) = delete;
};
class TileStreamer
{
public:
    struct UpdateCommand {
        uint2 coord;
        uint* level;
    };
    class LoadCallback
    {
    protected:
        ~LoadCallback() = default;

    public:
        virtual void load_tile(uint2 tile_index, uint level) = 0;
        virtual void unload_tile(uint2 tile_index, uint level) = 0;
        virtual bool can_load(uint2 tile_index, uint level) = 0;
    };

private:
    TileNodes _nodes;
    vstd::vector<uint8_t> _tile_mip_levels;
    uint2 _resolution;
    uint _mip;

public:
    [[nodiscard]] auto count() const { return _nodes.count(); }
    auto resolution() const { return _resolution; }
    TileNodes::Node& sample_node(uint2 tile_index, uint level);
    void load_tile(
        LoadCallback& callback,
        uint2 tile_index,
        uint level
    );
    // The minimum level can load
    uint can_load(
        LoadCallback& callback,
        uint2 tile_index,
        uint level
    );

    uint8_t tile_level(uint2 tile_idx) const;
    uint8_t& frame_lru(uint2 tile_index);
    TileStreamer(
        uint2 resolution,
        uint mip,
        bool enable_frame_lru
    );
    ~TileStreamer();
    TileStreamer(TileStreamer const&) = delete;
    TileStreamer(TileStreamer&&) = delete;
};
} // namespace rbc