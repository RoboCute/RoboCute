#pragma once
#include <rbc_config.h>
#include <luisa/ast/type.h>
#include <luisa/runtime/byte_buffer.h>
#include <luisa/vstl/common.h>
#include <rbc_node/buffer_desc.h>
namespace rbc {
enum struct ComputeDeviceType : uint32_t {
    HOST,//
    REMOTE,
    RENDER_DEVICE,
    COMPUTE_DEVICE
};
struct alignas(sizeof(uint64_t)) ComputeDeviceDesc {
    ComputeDeviceType type;
    uint32_t device_index;
    bool operator==(ComputeDeviceDesc const &rhs) const {
        return reinterpret_cast<uint64_t const&>(*this) == reinterpret_cast<uint64_t const&>(rhs);
    }
    bool operator<(ComputeDeviceDesc const &rhs) const {
        return reinterpret_cast<uint64_t const&>(*this) < reinterpret_cast<uint64_t const&>(rhs);
    }
};

struct NodeBuffer {
private:
    RC<BufferDescriptor> _buffer_desc;
    ComputeDeviceDesc _src_device_desc;
    ComputeDeviceDesc _dst_device_desc;
    vstd::variant<
        ByteBuffer,
        luisa::vector<std::byte>>
        _buffer;
public:
    NodeBuffer(RC<BufferDescriptor> buffer_desc, ComputeDeviceDesc const &src_device_desc, ComputeDeviceDesc const &dst_device_desc);
    BufferDescriptor const &buffer_desc() const {
        return *_buffer_desc;
    }
    auto src_device_desc() const { return _src_device_desc; }
    auto dst_device_desc() const { return _dst_device_desc; }
    ~NodeBuffer();
};
}// namespace rbc