#include <rbc_config.h>
#include <rbc_node/node_buffer.h>
namespace rbc {
NodeBuffer::NodeBuffer(RC<BufferDescriptor> &&buffer_desc,
                       ComputeDeviceDesc const &src_device_desc,
                       ComputeDeviceDesc const &dst_device_desc)
    : _buffer_desc(std::move(buffer_desc)),
      _src_device_desc(src_device_desc),
      _dst_device_desc(dst_device_desc) {}
NodeBuffer::~NodeBuffer() {}
BufferDescriptor::BufferDescriptor(
    Type const *element_type,
    size_t element_size)
    : _element_type(element_type),
      _element_size(element_size) {}
BufferDescriptor::~BufferDescriptor() {}
size_t BufferDescriptor::size_bytes() const {
    return _element_size * _element_type->size();
}
}// namespace rbc