#pragma once
#include <rbc_config.h>
#include <luisa/ast/type.h>
#include <rbc_core/rc.h>
namespace rbc {
using namespace luisa::compute;

struct RBC_NODE_API BufferDescriptor : RCBase {
private:
    Type const *_element_type;
    size_t _element_size;

public:
    BufferDescriptor(
        Type const *element_type,
        size_t element_size);
    ~BufferDescriptor();
    size_t size_bytes() const;
};
}// namespace rbc