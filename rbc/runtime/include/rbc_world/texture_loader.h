#pragma once
#include <rbc_world/resources/texture.h>
#include <rbc_graphics/texture/pack_texture.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/event.h>
#include <luisa/vstl/lockfree_array_queue.h>
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
    void process_texture(RC<TextureResource> const &tex, uint mip_level, bool to_vt);
    void finish_task();
};
}// namespace rbc::world