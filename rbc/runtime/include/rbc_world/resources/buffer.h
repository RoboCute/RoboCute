#pragma once
#include <rbc_world/resource_base.h>
#include <luisa/runtime/buffer.h>

namespace rbc {
struct DeviceBuffer;
}// namespace rbc

namespace rbc::world {

struct RBC_RUNTIME_API BufferResource final : ResourceBaseImpl<BufferResource> {
    DECLARE_WORLD_OBJECT_FRIEND(BufferResource)
    using BaseType = ResourceBaseImpl<BufferResource>;

private:
    RC<DeviceBuffer> _device_buffer;
    uint64_t _size_bytes{};
    bool _create_device_buffer{};
    mutable rbc::shared_atomic_mutex _async_mtx;

    BufferResource();
    ~BufferResource();

public:
    bool empty() const;
    [[nodiscard]] auto size_bytes() const { return _size_bytes; }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    [[nodiscard]] DeviceBuffer *device_buffer() const;
    [[nodiscard]] luisa::compute::BufferView<uint> buffer() const;
    void create_empty(uint64_t size_bytes, bool create_device_buffer);

    rbc::coroutine _async_load() override;
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;

protected:
    bool _install() override { return false; }
    bool unsafe_save_to_path() const override;
};

}// namespace rbc::world

RBC_RTTI(rbc::world::BufferResource)
