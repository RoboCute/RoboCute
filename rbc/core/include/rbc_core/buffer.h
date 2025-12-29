#pragma once

namespace rbc {

enum class EVertexAttribute : uint32_t {
    NONE,
    POSITION,
    NORMAL,
    TANGENT,
    TEXCOORD,
    COLOR,
    JOINT_INDEX,
    JOINT_WEIGHT,
    CUSTOM,
    SIZE,
    MAX_ENUM_BIT = UINT32_MAX
};

struct VertexBufferEntry {
    EVertexAttribute attribute;
    uint32_t buffer_index;
    uint32_t vertex_count;
    uint32_t stride;
    uint32_t offset;
};

// struct VertexBufferView {
// };

}// namespace rbc