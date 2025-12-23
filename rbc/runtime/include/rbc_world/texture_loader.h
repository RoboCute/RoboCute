#pragma once
#include <rbc_world/resources/texture.h>
#include <rbc_graphics/texture/pack_texture.h>
namespace rbc::world {
struct RBC_RUNTIME_API TextureLoader {
private:
    vstd::optional<PackTexture> _pack_tex;
    CommandList _cmdlist;
    TimelineEvent _event;
    std::atomic_uint64_t _finished_fence{1};
    vstd::SingleThreadArrayQueue<std::pair<vstd::function<void()>, uint64_t>> _after_device_task;
    std::mutex _mtx;
    luisa::fiber::counter _counter;
    void _try_execute();

public:
    RC<TextureResource> decode_texture(
        luisa::filesystem::path const &path,
        uint mip_level,
        bool to_vt);
    void finish_task();
};
}// namespace rbc::world