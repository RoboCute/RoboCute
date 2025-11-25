#pragma once
#include "rbc_world/generated/resource_type.hpp"

namespace rbc {
using ResourceID = uint32_t;
constexpr ResourceID INVALID_RESOURCE = 0;

// 资源元数据
struct ResourceMetadata {
    ResourceID id;
    ResourceType type;
    ResourceState state;
    std::string path;         // 资源文件路径
    std::string name;         // 资源名称
    size_t memory_size;       // 占用内存大小(字节)
    uint32_t reference_count; // 引用计数
    uint64_t last_access_time;// 最后访问时间(用于LRU)

    // 类型特定数据
};

}// namespace rbc