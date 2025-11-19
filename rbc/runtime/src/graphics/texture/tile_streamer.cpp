#include <rbc_graphics/texture/tile_streamer.h>
#include <luisa/core/logging.h>
namespace rbc
{
size_t TileNodes::_idx(uint2 tile_index, uint level)
{
    size_t offset{ 0 };
    uint2 res = _resolution;
    for (auto i : vstd::range(level))
    {
        offset += res.x * res.y;
        res >>= 1u;
    }
    return offset + res.x * tile_index.y + tile_index.x;
}
size_t TileNodes::_offset(uint level)
{
    size_t offset{ 0 };
    uint2 res = _resolution;
    for (auto i : vstd::range(level))
    {
        offset += res.x * res.y;
        res >>= 1u;
    }
    return offset;
}
TileNodes::TileNodes(uint2 resolution, uint mip)
    : _resolution(resolution)
    , _mip(mip)
{
    size_t sz{ 0 };
    uint2 res = resolution;
    for (auto i : vstd::range(mip))
    {
        LUISA_ASSERT(res.x >= 1 && res.y >= 1, "Resolution must be larger than 0.");
        sz += res.x * res.y;
        res >>= 1u;
    }
    _nodes.push_back_uninitialized(sz);
    memset(_nodes.data(), 0, _nodes.size_bytes());
}
TileNodes::~TileNodes() {}

auto TileNodes::sample_node(uint2 tile_index, uint level) -> Node&
{
    return _nodes[_idx(tile_index, level)];
}

bool TileNodes::try_load_chunk(uint2 tile_index, uint level)
{
    auto& nd = sample_node(tile_index, level);
    auto id = nd.ref_count++;
    return id == 0;
}
bool TileNodes::try_unload_chunk(uint2 tile_index, uint level)
{
    auto& nd = sample_node(tile_index, level);
    if (nd.ref_count == 0)
    {
        return false;
    }
    return (--nd.ref_count) == 0;
}
bool TileNodes::require_load(uint2 tile_index, uint level)
{
    auto& nd = sample_node(tile_index, level);
    return nd.ref_count == 0;
}

TileStreamer::TileStreamer(
    uint2 resolution,
    uint mip,
    bool enable_frame_lru)
    : _nodes(resolution, mip)
    , _resolution{ resolution }
    , _mip{ mip }
{
    if (mip > 8) [[unlikely]]
    {
        LUISA_ERROR("Mip can not larger than 8.");
    }
    _tile_mip_levels.push_back_uninitialized(_resolution.x * _resolution.y * (enable_frame_lru ? 2 : 1));
    memset(_tile_mip_levels.data(), mip, _resolution.x * _resolution.y);
    if (enable_frame_lru)
        memset(_tile_mip_levels.data() + _resolution.x * _resolution.y, 0, _resolution.x * _resolution.y);
}

uint8_t& TileStreamer::frame_lru(uint2 tile_idx)
{
    return _tile_mip_levels[tile_idx.y * _resolution.x + tile_idx.x + _resolution.x * _resolution.y];
}

uint8_t TileStreamer::tile_level(uint2 tile_idx) const
{
    return _tile_mip_levels[tile_idx.y * _resolution.x + tile_idx.x];
}

TileNodes::Node& TileStreamer::sample_node(uint2 tile_index, uint level)
{
    return _nodes.sample_node(tile_index, level);
}
void TileStreamer::load_tile(
    LoadCallback& callback,
    uint2 tile_index,
    uint level)
{
    if (level > _mip - 1) [[unlikely]]
    {
        LUISA_ERROR("Illegal level");
    }
    auto& old_level = _tile_mip_levels[tile_index.y * _resolution.x + tile_index.x];
    if (old_level > level)
    {
        for (auto i : vstd::range(level, old_level))
        {
            auto mip_tile = tile_index >> uint2(i);
            if (_nodes.try_load_chunk(mip_tile, i))
            {
                callback.load_tile(mip_tile, i);
            }
        }
    }
    // Unload tile logic
    else if (old_level < level)
    {
        for (auto i : vstd::range(old_level, level))
        {
            auto mip_tile = tile_index >> uint2(i);
            if (_nodes.try_unload_chunk(mip_tile, i))
            {
                callback.unload_tile(mip_tile, i);
            }
        }
    }
    old_level = level;
}
uint TileStreamer::can_load(
    LoadCallback& callback,
    uint2 tile_index,
    uint level)
{
    if (level > _mip - 1) [[unlikely]]
    {
        LUISA_ERROR("Illegal level");
    }
    auto& old_level = _tile_mip_levels[tile_index.y * _resolution.x + tile_index.x];
    if (old_level <= level) return level;
    uint dst_level = std::min<int>(old_level, _mip - 1);
    for (int i = dst_level; i >= static_cast<int>(level); --i)
    {
        auto mip_tile = tile_index >> uint2(i);
        if (_nodes.require_load(mip_tile, i))
        {
            if (!callback.can_load(mip_tile, i)) return dst_level;
        }
        dst_level = i;
    }
    return dst_level;
}
TileStreamer::~TileStreamer() {}
} // namespace rbc