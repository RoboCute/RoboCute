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
│  │  - 资源注册表 (Registry)                                  │  │
│  │  - 资源句柄管理 (Handle Management)                       │  │
│  │  - 请求队列管理 (Request Queue)                           │  │
│  │  - 生命周期管理 (Lifecycle)                               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                          ↓ enqueue                             │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           ResourceRequestQueue (Thread-Safe)             │  │
│  │  - 多生产者单消费者队列                                    │  │
│  │  - 优先级支持                                             │  │
│  └──────────────────────────────────────────────────────────┘  │
│                          ↓                                     │
└────────────────────────────────────────────────────────────────┘
                           ↓ dequeue (background thread)
┌────────────────────────────────────────────────────────────────┐
│                   C++ Extension (Worker Thread)                │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │           AsyncResourceLoader (C++)                      │  │
│  │  - I/O 线程池                                             │  │
│  │  - 资源解析器 (Mesh/Texture/Material)                     │  │
│  │  - GPU上传队列                                            │  │
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

### 资源标识和元数据
### 资源请求
### ResourceManager核心类

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
