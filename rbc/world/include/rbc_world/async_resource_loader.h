#pragma once

#include "rbc_world/resource.h"
#include "rbc_world/resource_request.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

namespace rbc {

class ResourceLoader;
class ResourceStorage;

/**
 * 异步资源加载器
 * - 管理I/O线程池
 * - 解析资源文件
 * - 上传到GPU (如果需要)
 */
class AsyncResourceLoader {
public:
    AsyncResourceLoader();
    ~AsyncResourceLoader();
};

}// namespace rbc