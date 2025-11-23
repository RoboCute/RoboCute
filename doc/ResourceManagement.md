# RoboCute 资源管理系统设计

## 概述

RoboCute资源管理系统是一个**Python-First、多线程、异步加载**的资源管理架构,用于支持Python Server与C++ Editor之间的资源同步。核心设计原则:

1. **Python为权威数据源**: 所有资源的生命周期由Python端管理
2. **异步加载**: 资源加载在后台线程进行,不阻塞主循环
3. **增量同步**: Server-Editor之间仅传输必要的资源元数据
4. **C++加速**: 资源的I/O密集型操作(解析、解码)在C++扩展中实现
5. **统一资源系统**: 不依赖DCC工具,使用自定义资源格式

---

## 系统架构

### 整体架构图

```
┌────────────────────────────────────────────────────────────────┐
│                      Python Server (Main Thread)               │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              ResourceManager (Python)                    │  │
│  │  - 资源注册表 (Registry)                                   │  │
│  │  - 资源句柄管理 (Handle Management)                        │  │
│  │  - 请求队列管理 (Request Queue)                            │  │
│  │  - 生命周期管理 (Lifecycle)                                │  │
│  └──────────────────────────────────────────────────────────┘  │
│                          ↓ enqueue                             │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           ResourceRequestQueue (Thread-Safe)             │  │
│  │  - 多生产者单消费者队列                                     │  │
│  │  - 优先级支持                                               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                          ↓                                     │
└────────────────────────────────────────────────────────────────┘
                           ↓ dequeue (background thread)
┌────────────────────────────────────────────────────────────────┐
│                   C++ Extension (Worker Thread)                │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           AsyncResourceLoader (C++)                      │  │
│  │  - I/O 线程池                                              │  │
│  │  - 资源解析器 (Mesh/Texture/Material)                      │  │
│  │  - GPU上传队列                                             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                          ↓ completed                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           ResourceStorage (C++)                          │  │
│  │  - 内存池管理                                               │  │
│  │  - GPU资源管理                                              │  │
│  │  - LRU缓存策略                                              │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                           ↓ sync (optional)
┌────────────────────────────────────────────────────────────────┐
│                    C++ Editor (Optional)                       │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           EditorResourceCache (C++)                      │  │
│  │  - 按需加载 (On-Demand)                                    │  │
│  │  - 本地缓存                                                 │  │
│  │  - 资源代理 (Proxy)                                        │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

---

## 核心组件设计

### 1. 资源标识和元数据

#### 1.1 资源ID和类型

```cpp
// C++ 端定义 (rbc/core/include/rbc_core/resource.h)
namespace rbc {

// 资源ID (64位: 32位类型 + 32位序列号)
using ResourceID = uint64_t;
constexpr ResourceID INVALID_RESOURCE = 0;

// 资源类型
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
    Custom = 1000  // 用户自定义资源起始值
};

// 资源状态
enum class ResourceState : uint8_t {
    Unloaded,      // 未加载
    Pending,       // 加载请求已提交
    Loading,       // 正在加载
    Loaded,        // 加载完成
    Failed,        // 加载失败
    Unloading      // 正在卸载
};

// 资源元数据
struct ResourceMetadata {
    ResourceID id;
    ResourceType type;
    ResourceState state;
    std::string path;              // 资源文件路径
    std::string name;              // 资源名称
    size_t memory_size;            // 占用内存大小(字节)
    uint32_t reference_count;      // 引用计数
    uint64_t last_access_time;     // 最后访问时间(用于LRU)
    
    // 类型特定数据
    nlohmann::json type_specific_data;
};

} // namespace rbc
```

```python
# Python 端定义 (src/rbc_ext/resource.py)
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, Dict, Any

class ResourceType(IntEnum):
    Unknown = 0
    Mesh = 1
    Texture = 2
    Material = 3
    Shader = 4
    Animation = 5
    Skeleton = 6
    PhysicsShape = 7
    Audio = 8
    Custom = 1000

class ResourceState(IntEnum):
    Unloaded = 0
    Pending = 1
    Loading = 2
    Loaded = 3
    Failed = 4
    Unloading = 5

@dataclass
class ResourceMetadata:
    id: int  # ResourceID
    type: ResourceType
    state: ResourceState
    path: str
    name: str
    memory_size: int = 0
    reference_count: int = 0
    last_access_time: float = 0.0
    type_specific_data: Optional[Dict[str, Any]] = None
```

#### 1.2 资源请求

```cpp
// C++ 端 (rbc/core/include/rbc_core/resource_request.h)
namespace rbc {

// 资源加载优先级
enum class LoadPriority : uint8_t {
    Critical = 0,    // 立即需要(阻塞主线程)
    High = 1,        // 高优先级(场景关键资源)
    Normal = 2,      // 正常优先级
    Low = 3,         // 低优先级(预加载)
    Background = 4   // 后台加载(最低优先级)
};

// 资源请求
struct ResourceRequest {
    ResourceID id;
    ResourceType type;
    std::string path;
    LoadPriority priority;
    
    // 加载选项
    struct LoadOptions {
        bool generate_mipmaps = true;        // 纹理: 生成mipmap
        bool compress_gpu = true;            // 纹理: GPU压缩
        bool compute_normals = false;        // 网格: 计算法线
        bool compute_tangents = false;       // 网格: 计算切线
        int lod_level = 0;                   // LOD级别
        nlohmann::json custom_options;       // 自定义选项
    } options;
    
    // 回调 (C++侧)
    using CompletionCallback = std::function<void(ResourceID, bool success)>;
    CompletionCallback on_complete;
    
    // 请求时间戳
    std::chrono::steady_clock::time_point timestamp;
    
    // 优先级比较器
    bool operator<(const ResourceRequest& other) const {
        if (priority != other.priority)
            return priority > other.priority;  // 优先级高的排前面
        return timestamp < other.timestamp;    // 同优先级,先请求的排前面
    }
};

} // namespace rbc
```

```python
# Python 端
from enum import IntEnum
from dataclasses import dataclass, field
from typing import Optional, Callable, Dict, Any
import time

class LoadPriority(IntEnum):
    Critical = 0
    High = 1
    Normal = 2
    Low = 3
    Background = 4

@dataclass
class LoadOptions:
    generate_mipmaps: bool = True
    compress_gpu: bool = True
    compute_normals: bool = False
    compute_tangents: bool = False
    lod_level: int = 0
    custom_options: Optional[Dict[str, Any]] = None

@dataclass(order=True)
class ResourceRequest:
    priority: LoadPriority = field(compare=True)
    timestamp: float = field(default_factory=time.time, compare=True)
    id: int = field(compare=False)
    type: ResourceType = field(compare=False)
    path: str = field(compare=False)
    options: LoadOptions = field(default_factory=LoadOptions, compare=False)
    on_complete: Optional[Callable[[int, bool], None]] = field(default=None, compare=False)
```

---

### 2. Python端资源管理器

#### 2.1 ResourceManager核心类

```python
# src/robocute/resource.py
from typing import Dict, Optional, List, Callable
import threading
import queue
from pathlib import Path
import weakref
import rbc_ext  # C++ extension

class ResourceManager:
    """
    Python端资源管理器
    - 管理资源的生命周期
    - 维护资源注册表
    - 调度异步加载请求
    """
    
    def __init__(self, cache_budget_mb: int = 2048):
        # 资源注册表
        self._resources: Dict[int, ResourceMetadata] = {}
        self._path_to_id: Dict[str, int] = {}
        self._next_resource_id: int = 1
        self._resource_lock = threading.RLock()
        
        # 请求队列 (优先级队列)
        self._request_queue = queue.PriorityQueue()
        
        # C++ 异步加载器
        self._cpp_loader = rbc_ext.AsyncResourceLoader()
        self._cpp_loader.set_cache_budget(cache_budget_mb * 1024 * 1024)
        
        # 加载线程
        self._loader_thread: Optional[threading.Thread] = None
        self._running = False
        
        # 回调管理
        self._completion_callbacks: Dict[int, List[Callable]] = {}
        
        # 引用追踪 (弱引用)
        self._handles: weakref.WeakValueDictionary = weakref.WeakValueDictionary()
        
    # === 启动/停止 ===
    
    def start(self):
        """启动资源管理器 (在app.is_running()时调用)"""
        if self._running:
            return
            
        self._running = True
        self._loader_thread = threading.Thread(
            target=self._loader_worker,
            name="ResourceLoader",
            daemon=True
        )
        self._loader_thread.start()
        print("[ResourceManager] Started")
    
    def stop(self):
        """停止资源管理器"""
        if not self._running:
            return
            
        self._running = False
        self._request_queue.put(None)  # 发送终止信号
        
        if self._loader_thread:
            self._loader_thread.join(timeout=5.0)
            
        self._cpp_loader.shutdown()
        print("[ResourceManager] Stopped")
    
    # === 资源注册 ===
    
    def register_resource(self, path: str, resource_type: ResourceType, 
                         name: Optional[str] = None) -> int:
        """
        注册资源 (不立即加载)
        
        Args:
            path: 资源文件路径
            resource_type: 资源类型
            name: 资源名称 (可选)
        
        Returns:
            ResourceID
        """
        path = str(Path(path).resolve())
        
        with self._resource_lock:
            # 检查是否已注册
            if path in self._path_to_id:
                return self._path_to_id[path]
            
            # 分配新ID
            resource_id = self._allocate_resource_id(resource_type)
            
            # 创建元数据
            metadata = ResourceMetadata(
                id=resource_id,
                type=resource_type,
                state=ResourceState.Unloaded,
                path=path,
                name=name or Path(path).stem
            )
            
            self._resources[resource_id] = metadata
            self._path_to_id[path] = resource_id
            
            return resource_id
    
    def _allocate_resource_id(self, resource_type: ResourceType) -> int:
        """分配资源ID (高32位=类型, 低32位=序列号)"""
        type_bits = int(resource_type) << 32
        serial = self._next_resource_id
        self._next_resource_id += 1
        return type_bits | serial
    
    # === 资源加载 ===
    
    def load_resource(self, resource_id: int, 
                     priority: LoadPriority = LoadPriority.Normal,
                     options: Optional[LoadOptions] = None,
                     on_complete: Optional[Callable[[int, bool], None]] = None) -> bool:
        """
        异步加载资源
        
        Args:
            resource_id: 资源ID
            priority: 加载优先级
            options: 加载选项
            on_complete: 完成回调
        
        Returns:
            是否成功提交加载请求
        """
        with self._resource_lock:
            if resource_id not in self._resources:
                print(f"[ResourceManager] Resource {resource_id} not found")
                return False
            
            metadata = self._resources[resource_id]
            
            # 已加载,直接返回
            if metadata.state == ResourceState.Loaded:
                if on_complete:
                    on_complete(resource_id, True)
                return True
            
            # 正在加载,添加回调
            if metadata.state in (ResourceState.Pending, ResourceState.Loading):
                if on_complete:
                    self._add_completion_callback(resource_id, on_complete)
                return True
            
            # 更新状态
            metadata.state = ResourceState.Pending
            
            # 创建请求
            request = ResourceRequest(
                priority=priority,
                id=resource_id,
                type=metadata.type,
                path=metadata.path,
                options=options or LoadOptions(),
                on_complete=on_complete
            )
            
            # 添加到回调列表
            if on_complete:
                self._add_completion_callback(resource_id, on_complete)
            
            # 提交到队列
            self._request_queue.put(request)
            
            return True
    
    def load_resource_sync(self, resource_id: int, 
                          options: Optional[LoadOptions] = None) -> bool:
        """
        同步加载资源 (阻塞直到加载完成)
        
        注意: 尽量避免使用,除非是Critical资源
        """
        event = threading.Event()
        success = [False]
        
        def on_complete(rid: int, s: bool):
            success[0] = s
            event.set()
        
        if not self.load_resource(resource_id, LoadPriority.Critical, options, on_complete):
            return False
        
        event.wait(timeout=30.0)  # 最多等待30秒
        return success[0]
    
    # === 便利函数 ===
    
    def load_mesh(self, path: str, priority: LoadPriority = LoadPriority.Normal,
                  on_complete: Optional[Callable[[int, bool], None]] = None) -> int:
        """加载网格资源"""
        resource_id = self.register_resource(path, ResourceType.Mesh)
        self.load_resource(resource_id, priority, on_complete=on_complete)
        return resource_id
    
    def load_texture(self, path: str, priority: LoadPriority = LoadPriority.Normal,
                    on_complete: Optional[Callable[[int, bool], None]] = None) -> int:
        """加载纹理资源"""
        resource_id = self.register_resource(path, ResourceType.Texture)
        self.load_resource(resource_id, priority, on_complete=on_complete)
        return resource_id
    
    def load_material(self, path: str, priority: LoadPriority = LoadPriority.Normal,
                     on_complete: Optional[Callable[[int, bool], None]] = None) -> int:
        """加载材质资源"""
        resource_id = self.register_resource(path, ResourceType.Material)
        self.load_resource(resource_id, priority, on_complete=on_complete)
        return resource_id
    
    # === 资源句柄 ===
    
    def get_handle(self, resource_id: int) -> Optional['ResourceHandle']:
        """
        获取资源句柄 (智能指针)
        - 自动管理引用计数
        - 句柄销毁时自动减少引用
        """
        with self._resource_lock:
            if resource_id not in self._resources:
                return None
            
            # 检查是否已存在句柄
            if resource_id in self._handles:
                return self._handles[resource_id]
            
            # 创建新句柄
            handle = ResourceHandle(self, resource_id)
            self._handles[resource_id] = handle
            
            # 增加引用计数
            self._resources[resource_id].reference_count += 1
            
            return handle
    
    def release_handle(self, resource_id: int):
        """释放资源句柄 (减少引用计数)"""
        with self._resource_lock:
            if resource_id not in self._resources:
                return
            
            metadata = self._resources[resource_id]
            metadata.reference_count = max(0, metadata.reference_count - 1)
            
            # 引用计数为0时,标记为可卸载
            if metadata.reference_count == 0:
                self._schedule_unload(resource_id)
    
    # === 资源查询 ===
    
    def get_metadata(self, resource_id: int) -> Optional[ResourceMetadata]:
        """获取资源元数据"""
        with self._resource_lock:
            return self._resources.get(resource_id)
    
    def get_state(self, resource_id: int) -> ResourceState:
        """获取资源状态"""
        with self._resource_lock:
            metadata = self._resources.get(resource_id)
            return metadata.state if metadata else ResourceState.Unloaded
    
    def is_loaded(self, resource_id: int) -> bool:
        """资源是否已加载"""
        return self.get_state(resource_id) == ResourceState.Loaded
    
    # === 资源卸载 ===
    
    def unload_resource(self, resource_id: int):
        """卸载资源"""
        with self._resource_lock:
            if resource_id not in self._resources:
                return
            
            metadata = self._resources[resource_id]
            
            # 仍有引用,不能卸载
            if metadata.reference_count > 0:
                print(f"[ResourceManager] Cannot unload {resource_id}: still referenced")
                return
            
            # 更新状态
            metadata.state = ResourceState.Unloading
            
            # 通知C++端卸载
            self._cpp_loader.unload_resource(resource_id)
            
            # 更新状态
            metadata.state = ResourceState.Unloaded
            metadata.memory_size = 0
    
    def _schedule_unload(self, resource_id: int):
        """调度延迟卸载 (避免立即卸载后又加载)"""
        # TODO: 实现延迟卸载策略
        pass
    
    def clear_unused(self):
        """清理未使用的资源"""
        with self._resource_lock:
            to_unload = [
                rid for rid, meta in self._resources.items()
                if meta.reference_count == 0 and meta.state == ResourceState.Loaded
            ]
            
            for rid in to_unload:
                self.unload_resource(rid)
            
            print(f"[ResourceManager] Unloaded {len(to_unload)} unused resources")
    
    # === 内部方法 ===
    
    def _loader_worker(self):
        """资源加载工作线程"""
        print("[ResourceLoader] Worker thread started")
        
        while self._running:
            try:
                # 获取请求 (阻塞)
                request = self._request_queue.get(timeout=1.0)
                
                if request is None:  # 终止信号
                    break
                
                # 处理请求
                self._process_request(request)
                
            except queue.Empty:
                continue
            except Exception as e:
                print(f"[ResourceLoader] Error: {e}")
        
        print("[ResourceLoader] Worker thread stopped")
    
    def _process_request(self, request: ResourceRequest):
        """处理资源加载请求"""
        resource_id = request.id
        
        # 更新状态
        with self._resource_lock:
            if resource_id not in self._resources:
                return
            self._resources[resource_id].state = ResourceState.Loading
        
        # 调用C++加载器
        try:
            success = self._cpp_loader.load_resource(
                resource_id,
                request.type.value,
                request.path,
                request.options.__dict__
            )
            
            # 更新状态
            with self._resource_lock:
                if resource_id in self._resources:
                    metadata = self._resources[resource_id]
                    metadata.state = ResourceState.Loaded if success else ResourceState.Failed
                    
                    if success:
                        # 更新内存大小
                        metadata.memory_size = self._cpp_loader.get_resource_size(resource_id)
                        metadata.last_access_time = time.time()
            
            # 调用回调
            self._invoke_completion_callbacks(resource_id, success)
            
        except Exception as e:
            print(f"[ResourceLoader] Failed to load {request.path}: {e}")
            
            with self._resource_lock:
                if resource_id in self._resources:
                    self._resources[resource_id].state = ResourceState.Failed
            
            self._invoke_completion_callbacks(resource_id, False)
    
    def _add_completion_callback(self, resource_id: int, callback: Callable):
        """添加完成回调"""
        if resource_id not in self._completion_callbacks:
            self._completion_callbacks[resource_id] = []
        self._completion_callbacks[resource_id].append(callback)
    
    def _invoke_completion_callbacks(self, resource_id: int, success: bool):
        """调用完成回调"""
        callbacks = self._completion_callbacks.pop(resource_id, [])
        for callback in callbacks:
            try:
                callback(resource_id, success)
            except Exception as e:
                print(f"[ResourceManager] Callback error: {e}")
    
    # === Editor同步 ===
    
    def get_all_metadata(self) -> List[ResourceMetadata]:
        """获取所有资源元数据 (用于Editor同步)"""
        with self._resource_lock:
            return list(self._resources.values())
    
    def serialize_metadata(self, resource_id: int) -> Optional[dict]:
        """序列化资源元数据 (用于Editor同步)"""
        with self._resource_lock:
            metadata = self._resources.get(resource_id)
            if not metadata:
                return None
            
            return {
                'id': metadata.id,
                'type': metadata.type.value,
                'state': metadata.state.value,
                'path': metadata.path,
                'name': metadata.name,
                'memory_size': metadata.memory_size,
                'reference_count': metadata.reference_count
            }


class ResourceHandle:
    """
    资源句柄 (智能指针)
    - 自动管理引用计数
    - 提供便利的访问接口
    """
    
    def __init__(self, manager: ResourceManager, resource_id: int):
        self._manager = manager
        self._resource_id = resource_id
    
    def __del__(self):
        """析构时释放引用"""
        self._manager.release_handle(self._resource_id)
    
    @property
    def id(self) -> int:
        return self._resource_id
    
    @property
    def metadata(self) -> Optional[ResourceMetadata]:
        return self._manager.get_metadata(self._resource_id)
    
    @property
    def is_loaded(self) -> bool:
        return self._manager.is_loaded(self._resource_id)
    
    def wait_until_loaded(self, timeout: float = 30.0) -> bool:
        """等待资源加载完成"""
        import time
        start = time.time()
        while time.time() - start < timeout:
            if self.is_loaded:
                return True
            time.sleep(0.01)
        return False
    
    def get_data(self):
        """获取资源数据 (通过C++接口)"""
        if not self.is_loaded:
            return None
        return self._manager._cpp_loader.get_resource_data(self._resource_id)
```

---

### 3. C++端异步加载器

#### 3.1 AsyncResourceLoader

```cpp
// rbc/runtime/include/rbc_runtime/async_resource_loader.h
#pragma once

#include <rbc_core/resource.h>
#include <rbc_core/resource_request.h>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

namespace rbc {

class ResourceLoader;  // 前向声明
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
    
    // === 初始化 ===
    void initialize(size_t num_threads = 4);
    void shutdown();
    
    // === 缓存预算 ===
    void set_cache_budget(size_t bytes);
    size_t get_cache_budget() const { return cache_budget_; }
    size_t get_memory_usage() const;
    
    // === 资源加载 ===
    bool load_resource(ResourceID id, ResourceType type, 
                      const std::string& path,
                      const nlohmann::json& options);
    
    bool is_loaded(ResourceID id) const;
    ResourceState get_state(ResourceID id) const;
    size_t get_resource_size(ResourceID id) const;
    
    // === 资源访问 ===
    template<typename T>
    std::shared_ptr<T> get_resource(ResourceID id);
    
    // 通用接口 (返回void*,给Python用)
    void* get_resource_data(ResourceID id);
    
    // === 资源卸载 ===
    void unload_resource(ResourceID id);
    void clear_unused_resources();
    
    // === 注册自定义加载器 ===
    using LoaderFactory = std::function<std::unique_ptr<ResourceLoader>(ResourceType)>;
    void register_loader(ResourceType type, LoaderFactory factory);
    
private:
    // 加载工作线程
    void worker_thread();
    
    // 根据类型获取加载器
    ResourceLoader* get_loader(ResourceType type);
    
    // 实际加载逻辑
    bool load_resource_impl(ResourceID id, ResourceType type,
                           const std::string& path,
                           const nlohmann::json& options);
    
    // 线程池
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
    
    // 资源存储
    std::unique_ptr<ResourceStorage> storage_;
    
    // 加载器工厂
    std::unordered_map<ResourceType, LoaderFactory> loader_factories_;
    std::unordered_map<ResourceType, std::unique_ptr<ResourceLoader>> loaders_;
    
    // 缓存管理
    size_t cache_budget_;
    mutable std::mutex mutex_;
};

/**
 * 资源加载器基类
 * - 每种资源类型实现自己的加载器
 */
class ResourceLoader {
public:
    virtual ~ResourceLoader() = default;
    
    virtual bool can_load(const std::string& path) const = 0;
    virtual std::shared_ptr<void> load(const std::string& path, 
                                       const nlohmann::json& options) = 0;
    virtual size_t get_resource_size(const std::shared_ptr<void>& resource) const = 0;
};

} // namespace rbc
```

#### 3.2 资源存储

```cpp
// rbc/runtime/src/runtime/resource_storage.h
#pragma once

#include <rbc_core/resource.h>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <list>

namespace rbc {

/**
 * 资源存储
 * - 内存池管理
 * - LRU缓存策略
 */
class ResourceStorage {
public:
    ResourceStorage(size_t budget_bytes);
    
    // === 存储/获取 ===
    void store(ResourceID id, std::shared_ptr<void> data, 
              ResourceType type, size_t size);
    std::shared_ptr<void> get(ResourceID id);
    bool contains(ResourceID id) const;
    
    // === 状态查询 ===
    ResourceState get_state(ResourceID id) const;
    void set_state(ResourceID id, ResourceState state);
    size_t get_size(ResourceID id) const;
    
    // === 移除 ===
    void remove(ResourceID id);
    void clear();
    
    // === 缓存管理 ===
    size_t get_memory_usage() const { return current_memory_usage_; }
    size_t get_budget() const { return budget_bytes_; }
    void set_budget(size_t bytes);
    
    // LRU淘汰
    void evict_lru();
    void evict_until_budget();
    
    // 访问更新 (用于LRU)
    void touch(ResourceID id);
    
private:
    struct ResourceEntry {
        ResourceID id;
        std::shared_ptr<void> data;
        ResourceType type;
        ResourceState state;
        size_t size;
        
        // LRU链表迭代器
        std::list<ResourceID>::iterator lru_iter;
    };
    
    std::unordered_map<ResourceID, ResourceEntry> entries_;
    std::list<ResourceID> lru_list_;  // 最近使用的在前面
    
    size_t budget_bytes_;
    std::atomic<size_t> current_memory_usage_{0};
    
    mutable std::shared_mutex mutex_;
};

} // namespace rbc
```

#### 3.3 网格加载器示例

```cpp
// rbc/runtime/src/runtime/mesh_loader.cpp
#include "mesh_loader.h"
#include <rbc_graphics/mesh.h>
#include <fstream>
#include <sstream>

namespace rbc {

class MeshLoader : public ResourceLoader {
public:
    bool can_load(const std::string& path) const override {
        // 支持多种格式
        return path.ends_with(".obj") || 
               path.ends_with(".fbx") ||
               path.ends_with(".gltf") ||
               path.ends_with(".rbm");  // RoboCute自定义格式
    }
    
    std::shared_ptr<void> load(const std::string& path, 
                              const nlohmann::json& options) override {
        if (path.ends_with(".rbm")) {
            return load_rbm(path, options);
        } else if (path.ends_with(".obj")) {
            return load_obj(path, options);
        } else if (path.ends_with(".gltf")) {
            return load_gltf(path, options);
        }
        // 其他格式...
        return nullptr;
    }
    
    size_t get_resource_size(const std::shared_ptr<void>& resource) const override {
        auto mesh = std::static_pointer_cast<Mesh>(resource);
        return mesh->get_memory_size();
    }
    
private:
    std::shared_ptr<Mesh> load_rbm(const std::string& path, 
                                   const nlohmann::json& options) {
        // 加载RoboCute自定义格式 (高效二进制格式)
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return nullptr;
        }
        
        auto mesh = std::make_shared<Mesh>();
        
        // 读取头部
        struct Header {
            uint32_t magic;  // 'RBM\0'
            uint32_t version;
            uint32_t vertex_count;
            uint32_t index_count;
            uint32_t flags;
        } header;
        
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (header.magic != 0x004D4252) {  // 'RBM\0'
            return nullptr;
        }
        
        // 读取顶点数据
        mesh->vertices.resize(header.vertex_count);
        file.read(reinterpret_cast<char*>(mesh->vertices.data()),
                 header.vertex_count * sizeof(Vertex));
        
        // 读取索引数据
        mesh->indices.resize(header.index_count);
        file.read(reinterpret_cast<char*>(mesh->indices.data()),
                 header.index_count * sizeof(uint32_t));
        
        // 计算法线和切线 (如果选项要求)
        if (options.value("compute_normals", false)) {
            mesh->compute_normals();
        }
        if (options.value("compute_tangents", false)) {
            mesh->compute_tangents();
        }
        
        // 上传到GPU
        mesh->upload_to_gpu();
        
        return mesh;
    }
    
    std::shared_ptr<Mesh> load_obj(const std::string& path,
                                   const nlohmann::json& options) {
        // 加载OBJ格式
        // TODO: 实现OBJ加载逻辑
        return nullptr;
    }
    
    std::shared_ptr<Mesh> load_gltf(const std::string& path,
                                    const nlohmann::json& options) {
        // 加载GLTF格式
        // TODO: 实现GLTF加载逻辑
        return nullptr;
    }
};

// 注册加载器
void register_mesh_loader(AsyncResourceLoader& loader) {
    loader.register_loader(ResourceType::Mesh, 
        [](ResourceType) { return std::make_unique<MeshLoader>(); });
}

} // namespace rbc
```

---

### 4. Server-Editor资源同步

#### 4.1 EditorService中的资源同步

```python
# src/robocute/editor_service.py (扩展)

class EditorService:
    def __init__(self, scene: 'RBCScene', resource_manager: ResourceManager):
        # ... 之前的代码 ...
        self.resource_manager = resource_manager
    
    # === 资源同步 ===
    
    def broadcast_resource_registered(self, resource_id: int):
        """广播资源注册事件"""
        metadata = self.resource_manager.serialize_metadata(resource_id)
        if not metadata:
            return
        
        message = {
            'event': 'resource_registered',
            'resource': metadata
        }
        self._broadcast(message)
    
    def broadcast_resource_loaded(self, resource_id: int):
        """广播资源加载完成事件"""
        metadata = self.resource_manager.serialize_metadata(resource_id)
        if not metadata:
            return
        
        message = {
            'event': 'resource_loaded',
            'resource': metadata
        }
        self._broadcast(message)
    
    def broadcast_resource_unloaded(self, resource_id: int):
        """广播资源卸载事件"""
        message = {
            'event': 'resource_unloaded',
            'resource_id': resource_id
        }
        self._broadcast(message)
    
    def sync_all_resources(self, editor_id: str):
        """同步所有资源元数据到指定Editor"""
        all_metadata = self.resource_manager.get_all_metadata()
        
        message = {
            'event': 'resources_full_sync',
            'resources': [
                self.resource_manager.serialize_metadata(meta.id)
                for meta in all_metadata
            ]
        }
        self._send_to_editor(editor_id, message)
    
    # === 命令处理扩展 ===
    
    def _handle_editor_command(self, command: dict):
        """处理Editor命令 (扩展)"""
        cmd_type = command['type']
        
        if cmd_type == 'request_resource_sync':
            # Editor请求资源同步
            editor_id = command['editor_id']
            self.sync_all_resources(editor_id)
            return {'success': True}
        
        elif cmd_type == 'load_resource':
            # Editor请求加载资源
            path = command['path']
            resource_type = ResourceType(command['resource_type'])
            priority = LoadPriority(command.get('priority', LoadPriority.Normal))
            
            resource_id = self.resource_manager.register_resource(path, resource_type)
            success = self.resource_manager.load_resource(resource_id, priority)
            
            return {'success': success, 'resource_id': resource_id}
        
        # ... 其他命令处理 ...
```

#### 4.2 Editor端资源缓存

```cpp
// Editor端资源缓存 (rbc/editor/editor_resource_cache.h)
#pragma once

#include <rbc_core/resource.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

namespace rbc::editor {

/**
 * Editor资源缓存
 * - 按需从Server请求资源
 * - 本地缓存用于渲染
 */
class EditorResourceCache {
public:
    EditorResourceCache(class ServerConnection& connection);
    
    // === 资源获取 (按需加载) ===
    std::shared_ptr<Mesh> get_mesh(ResourceID id);
    std::shared_ptr<Texture> get_texture(ResourceID id);
    std::shared_ptr<Material> get_material(ResourceID id);
    
    // === 从Server同步元数据 ===
    void on_resource_registered(const nlohmann::json& metadata);
    void on_resource_loaded(const nlohmann::json& metadata);
    void on_resource_unloaded(ResourceID id);
    void on_full_resource_sync(const std::vector<nlohmann::json>& resources);
    
    // === 本地加载 (Editor独立加载,不依赖Server) ===
    std::shared_ptr<Resource> load_local(ResourceID id, const std::string& path);
    
    // === 缓存管理 ===
    void clear_cache();
    void set_cache_budget(size_t bytes);
    size_t get_memory_usage() const;
    
private:
    struct CachedResource {
        ResourceMetadata metadata;
        std::shared_ptr<void> data;
        bool loaded_locally;  // 是否在Editor本地加载
    };
    
    ServerConnection& connection_;
    
    // 资源元数据 (从Server同步)
    std::unordered_map<ResourceID, ResourceMetadata> metadata_cache_;
    
    // 资源数据缓存 (Editor本地加载)
    std::unordered_map<ResourceID, CachedResource> data_cache_;
    
    // 本地加载器
    std::unique_ptr<AsyncResourceLoader> local_loader_;
    
    mutable std::shared_mutex mutex_;
    
    // 请求Server加载资源
    bool request_load_from_server(ResourceID id);
    
    // 本地加载资源 (fallback)
    std::shared_ptr<void> load_locally(ResourceID id, ResourceType type, 
                                       const std::string& path);
};

} // namespace rbc::editor
```

---

## 使用示例

### 1. Python端基本使用

```python
import robocute as rbc

# 创建场景和资源管理器
scene = rbc.Scene()
resource_mgr = rbc.ResourceManager(cache_budget_mb=2048)

# 启动资源管理器
resource_mgr.start()

# === 注册和加载资源 ===

# 方式1: 先注册,后加载
mesh_id = resource_mgr.register_resource("models/robot.rbm", rbc.ResourceType.Mesh)
resource_mgr.load_resource(mesh_id, priority=rbc.LoadPriority.High)

# 方式2: 便利函数 (注册+加载)
texture_id = resource_mgr.load_texture("textures/metal.png", 
                                       priority=rbc.LoadPriority.Normal)

# 方式3: 带回调
def on_material_loaded(resource_id, success):
    if success:
        print(f"Material {resource_id} loaded successfully")
    else:
        print(f"Failed to load material {resource_id}")

material_id = resource_mgr.load_material("materials/pbr_metal.json",
                                        on_complete=on_material_loaded)

# === 使用资源句柄 ===
mesh_handle = resource_mgr.get_handle(mesh_id)

# 等待加载完成
if mesh_handle.wait_until_loaded(timeout=10.0):
    # 获取网格数据
    mesh_data = mesh_handle.get_data()
    print(f"Mesh vertices: {mesh_data.vertex_count}")
else:
    print("Timeout waiting for mesh to load")

# === 创建实体并使用资源 ===
robot = scene.create_entity("Robot")
scene.add_component(robot, rbc.TransformComponent(position=[0, 0, 1]))
scene.add_component(robot, rbc.RenderComponent(mesh_id=mesh_id, 
                                               material_ids=[material_id]))

# === 主循环 ===
while scene.is_running():
    scene.update(dt=0.016)
    
# 清理
resource_mgr.stop()
```

### 2. 带Editor的使用

```python
import robocute as rbc

# 创建场景和资源管理器
scene = rbc.Scene()
resource_mgr = rbc.ResourceManager()
resource_mgr.start()

# 启动Editor服务
editor_service = rbc.EditorService(scene, resource_mgr)
editor_service.start(port=5555)

# 启动Editor GUI
editor_process = rbc.launch_editor()

# 主循环
try:
    while scene.is_running():
        # 处理Editor命令 (包括资源加载请求)
        editor_service.process_commands()
        
        # 更新场景
        scene.update(dt=0.016)
        
        # 推送状态到Editor
        editor_service.push_simulation_state()
        
except KeyboardInterrupt:
    print("Shutting down...")
    
finally:
    # 清理
    editor_service.stop()
    resource_mgr.stop()
    editor_process.terminate()
```

### 3. 自定义资源类型

```python
# 注册自定义资源加载器
import rbc_ext

class CustomResourceLoader(rbc_ext.ResourceLoader):
    def can_load(self, path: str) -> bool:
        return path.endswith('.custom')
    
    def load(self, path: str, options: dict):
        # 自定义加载逻辑
        with open(path, 'rb') as f:
            data = f.read()
        
        # 解析数据
        custom_data = self.parse_custom_format(data)
        
        return custom_data
    
    def get_resource_size(self, resource) -> int:
        return len(resource.data)

# 注册到资源管理器
resource_mgr.register_custom_loader(
    resource_type=rbc.ResourceType.Custom + 1,
    loader=CustomResourceLoader()
)

# 使用自定义资源
custom_id = resource_mgr.register_resource("data/custom.custom", 
                                          rbc.ResourceType.Custom + 1)
resource_mgr.load_resource(custom_id)
```

---

## 性能优化策略

### 1. 批量加载

```python
# 批量注册和加载资源
def load_scene_resources(resource_mgr, scene_file: str):
    """从场景文件批量加载资源"""
    import json
    
    with open(scene_file, 'r') as f:
        scene_data = json.load(f)
    
    resource_ids = []
    
    # 批量注册
    for resource_ref in scene_data['resources']:
        rid = resource_mgr.register_resource(
            resource_ref['path'],
            rbc.ResourceType(resource_ref['type'])
        )
        resource_ids.append(rid)
    
    # 批量加载 (高优先级)
    for rid in resource_ids:
        resource_mgr.load_resource(rid, priority=rbc.LoadPriority.High)
    
    return resource_ids
```

### 2. 预加载和LOD

```python
# 预加载和LOD策略
def preload_with_lod(resource_mgr, base_path: str):
    """预加载多个LOD级别"""
    lod_levels = [
        (0, rbc.LoadPriority.Critical),   # LOD0: 最高优先级
        (1, rbc.LoadPriority.High),       # LOD1: 高优先级
        (2, rbc.LoadPriority.Normal),     # LOD2: 正常优先级
        (3, rbc.LoadPriority.Background)  # LOD3: 后台加载
    ]
    
    lod_ids = []
    
    for lod_level, priority in lod_levels:
        path = f"{base_path}_lod{lod_level}.rbm"
        options = rbc.LoadOptions(lod_level=lod_level)
        
        rid = resource_mgr.register_resource(path, rbc.ResourceType.Mesh)
        resource_mgr.load_resource(rid, priority=priority, options=options)
        lod_ids.append(rid)
    
    return lod_ids
```

### 3. 内存预算管理

```python
# 动态调整内存预算
def manage_memory_budget(resource_mgr, scene):
    """根据场景复杂度动态调整内存预算"""
    entity_count = len(scene.get_all_entities())
    
    if entity_count < 1000:
        budget_mb = 1024  # 1GB
    elif entity_count < 10000:
        budget_mb = 2048  # 2GB
    else:
        budget_mb = 4096  # 4GB
    
    resource_mgr._cpp_loader.set_cache_budget(budget_mb * 1024 * 1024)
    
    # 定期清理未使用资源
    resource_mgr.clear_unused()
```

---

## 总结

### 核心特性

1. **异步加载**: 资源加载在后台线程进行,不阻塞主循环
2. **智能缓存**: LRU策略自动管理内存,支持可配置的内存预算
3. **引用计数**: 自动管理资源生命周期,避免内存泄漏
4. **增量同步**: Server-Editor之间仅传输必要的元数据
5. **可扩展**: 支持注册自定义资源类型和加载器
6. **多线程安全**: 所有公共接口都是线程安全的

### 技术亮点

1. **Python-C++混合**: Python管理逻辑,C++处理I/O密集型操作
2. **优先级队列**: 支持关键资源优先加载
3. **乐观更新**: Editor端可以立即预览,Server端异步确认
4. **弱引用追踪**: 智能指针自动管理引用计数
5. **统一资源系统**: 不依赖DCC工具,使用自定义高效格式

### 与Editor.md的整合

- ResourceManager作为Scene的一部分,由Python Server管理
- EditorService负责将资源状态同步到Editor
- Editor通过命令请求加载资源,Server决定是否加载
- Editor本地缓存资源数据,用于渲染和可视化

这个架构完全符合RoboCute的**Python-First**理念,同时提供了高性能的资源管理能力!
