#pragma once
#include "rbc_world/resource.h"
#include "rbc_world/generated/resource_type.hpp"
#include <functional>
#include <chrono>

namespace rbc {

enum class LoadPriority : uint8_t {
    Critical = 0, // 立即需要(阻塞主线程)
    High = 1,     // 高优先级(场景关键资源)
    Normal = 2,   // 正常优先级
    Low = 3,      // 低优先级(预加载)
    Background = 4// 后台加载(最低优先级)
};

struct ResourceRequest {
    ResourceID id;
    ResourceType type;
    std::string path;
    LoadPriority priority;
    // 加载选项
    struct LoadOptions {
        bool generate_mipmaps = true; // 纹理: 生成mipmap
        bool compress_gpu = true;     // 纹理: GPU压缩
        bool compute_normals = false; // 网格: 计算法线
        bool compute_tangents = false;// 网格: 计算切线
        int lod_level = 0;            // LOD级别
        // nlohmann::json custom_options;// 自定义选项
    } options;

    using CompletionCallback = std::function<void(ResourceID, bool success)>;
    CompletionCallback on_complete;
    std::chrono::steady_clock::time_point timestamp;

    // 优先级比较器
    bool operator<(const ResourceRequest &other) const {
        if (priority != other.priority)
            return priority > other.priority;// 优先级高的排前面
        return timestamp < other.timestamp;  // 同优先级,先请求的排前面
    }
};

}// namespace rbc