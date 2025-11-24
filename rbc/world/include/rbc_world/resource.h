#pragma once
namespace rbc {
using ResourceID = uint32_t;
constexpr ResourceID INVALID_RESOURCE = 0;

enum class ResourceType : uint32_t {
    Unknown = 0,
    Mesh = 1,
    Texture = 2,
    Material = 3,
    Shader = 4,
    Animation = 5,
    Skeleton = 6,
    PhysicsShape = 7,
    Audio = 8,
    Custom = 1000// 用户自定义资源起始值
};

enum class ResourceState : uint8_t {
    Unloaded, // 未加载
    Pending,  // 加载请求已提交
    Loading,  // 正在加载
    Loaded,   // 加载完成（serde类结构加载从本地磁盘/内存/网络加载完成）
    Installed,// 安装完成（上传GPU，依赖处理等完成，可以被其他组件直接使用）
    Failed,   // 加载失败
    Unloading // 正在卸载
};

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