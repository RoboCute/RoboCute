---
name: world_resource
description: How to write a world resource class in RoboCute
triggers:
  - file_types: [".cpp", ".h"]
  - patterns:
    - "rbc_world/resources"
    - "ResourceBaseImpl"
---

# Writing a World Resource Class

This guide explains how to create a world resource class in the RoboCute engine. World resources are assets that can be loaded asynchronously, saved to disk, and managed by the resource system.

## Overview

World resources inherit from `ResourceBaseImpl<T>` and follow a specific lifecycle pattern. They represent assets like meshes, materials, buffers, textures, etc. that are referenced by entities and components.

## Resource Lifecycle

Resources follow a loading status progression:

```
Unloaded -> Loading -> Loaded -> Installing -> Installed
```

- **Unloaded**: Initial state, no data loaded
- **Loading**: `_async_load()` coroutine is running
- **Loaded**: Host-side data is ready
- **Installing**: `_install()` is setting up device resources
- **Installed**: Fully ready for use (host + device)

## Minimal Resource Class Template

### Header File (`.h`)

```cpp
#pragma once
#include <rbc_world/resource_base.h>

// Forward declarations for device-side resource (if any)
namespace rbc {
struct DeviceMyResource;
}

namespace rbc::world {

struct RBC_RUNTIME_API MyResource final : ResourceBaseImpl<MyResource> {
    DECLARE_WORLD_OBJECT_FRIEND(MyResource)
    using BaseType = ResourceBaseImpl<MyResource>;

private:
    // Device resource reference (optional, for GPU-side data) [UNNECESSARY]
    RC<DeviceMyResource> _device_resource;
    
    // Metadata fields [UNNECESSARY]
    uint64_t _size_bytes{};
    bool _some_flag : 1 {false};
    
    // Thread safety
    mutable rbc::shared_atomic_mutex _async_mtx;

    // Private constructor/destructor (required)
    MyResource();
    ~MyResource();

public:
    // Public interface
    [[nodiscard]] bool empty() const;
    // Getter, Setter
    [[nodiscard]] auto size_bytes() const { return _size_bytes; }
    [[nodiscard]] auto some_flag() const { return _some_flag; }
    
    // Factory method to create empty resource
    void create_empty(uint64_t size_bytes);
    
    // Optional: serialization for meta file
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;

    // Required: async loading coroutine
    rbc::coroutine _async_load() override;

protected:
    // Required: install device resources
    bool _install() override;
    
    // Required: save to file path
    bool unsafe_save_to_path() const override;
};

}// namespace rbc::world

// Required: RTTI registration
RBC_RTTI(rbc::world::MyResource)
```

### Implementation File (`.cpp`)

```cpp
#include <rbc_world/resources/my_resource.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/type_register.h>

namespace rbc::world {

MyResource::MyResource() = default;

MyResource::~MyResource() = default;

bool MyResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return !_device_resource;
}

void MyResource::create_empty(uint64_t size_bytes) {
    // Wait for any ongoing loading
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Last resource loading.");
    }
    
    // Reset status
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);
    
    std::lock_guard lck{_async_mtx};
    _device_resource.reset();
    _device_resource = new DeviceMyResource{};
    _size_bytes = size_bytes;
    
    // Mark as loaded (host-side ready)
    unsafe_set_loaded();
}

void MyResource::serialize_meta(ObjSerialize const &obj) const {
    std::shared_lock lck{_async_mtx};
    obj.ar.value(_size_bytes, "size_bytes");
    obj.ar.value(_some_flag, "some_flag");
}

void MyResource::deserialize_meta(ObjDeSerialize const &obj) {
    std::lock_guard lck{_async_mtx};
    uint64_t size_bytes = 0;
    if (obj.ar.value(size_bytes, "size_bytes")) {
        _size_bytes = size_bytes;
    }
    bool some_flag = false;
    if (obj.ar.value(some_flag, "some_flag")) {
        _some_flag = some_flag;
    }
}

rbc::coroutine MyResource::_async_load() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;
    
    auto path = this->path();
    if (path.empty()) {
        co_return;
    }
    
    std::lock_guard lck{_async_mtx};
    if (_device_resource) {
        co_return;  // Already loaded
    }
    
    // Create device resource and start async loading
    _device_resource = new DeviceMyResource{};
    _device_resource->async_load_from_file(path, _size_bytes);
    
    // Yield until loading completes
    while (!_device_resource->load_finished()) {
        co_await std::suspend_always{};
    }
    
    // Mark as installed (fully ready)
    unsafe_set_installed();
    co_return;
}

bool MyResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    
    std::lock_guard lck{_async_mtx};
    if (!_device_resource) return false;
    
    // Upload to GPU, create buffers, etc.
    // Return true if installation succeeds
    return _device_resource->upload_to_device(render_device->lc_main_cmd_list());
}

bool MyResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_device_resource || _device_resource->host_data().empty()) {
        return false;
    }
    
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_device_resource->host_data());
    return true;
}

// Required: Register the resource type
DECLARE_WORLD_OBJECT_REGISTER(MyResource)

}// namespace rbc::world
```

## Key Patterns and Guidelines

### 1. Class Declaration

- **Inherit from** `ResourceBaseImpl<YourClass>` using CRTP pattern
- **Use `final`** keyword to prevent further inheritance
- **Declare** `DECLARE_WORLD_OBJECT_FRIEND(YourClass)` to allow object pool creation
- **Define** `using BaseType = ResourceBaseImpl<YourClass>;` for convenience
- **Mark with** `RBC_RUNTIME_API` for DLL export
- **End with** `RBC_RTTI(rbc::world::YourClass)` macro for RTTI

### 2. Member Variables

```cpp
private:
    // Device resource (if GPU-side data needed) [UNNECESSARY]
    RC<DeviceResource> _device_res;
    
    // Thread safety - always use shared_atomic_mutex
    mutable rbc::shared_atomic_mutex _async_mtx;
    
    // Metadata - loaded from meta file before _async_load() [UNNECESSARY]
    uint64_t _size_bytes{};
    bool _some_flag : 1 {false};
```

### 3. Constructor/Destructor

```cpp
// Keep private, friends can create via object pool
MyResource();
~MyResource();

// In .cpp file:
MyResource::MyResource() = default;
MyResource::~MyResource() {
    // Cleanup: dispose device resources
    if (_device_res) {
        auto inst = AssetsManager::instance();
        if (inst) {
            inst->dispose_after_render_frame(std::move(_device_res));
        }
    }
}
```

### 4. Async Loading (`_async_load`)

The `_async_load()` method is a coroutine that loads resource data asynchronously:

```cpp
rbc::coroutine MyResource::_async_load() {
    // Get render device (return early if not available)
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;
    
    auto path = this->path();
    if (path.empty()) co_return;
    
    // Lock and check if already loaded
    std::lock_guard lck{_async_mtx};
    if (_device_res) co_return;
    
    // Load from file
    _device_res = new DeviceResource{};
    _device_res->async_load_from_file(path, ...);
    
    // Yield loop - wait for async file I/O
    while (!_device_res->load_finished()) {
        co_await std::suspend_always{};
    }
    
    // Mark as installed when done
    unsafe_set_installed();
    co_return;
}
```

**For resources with dependencies** (e.g., material depending on textures):

```cpp
rbc::coroutine MaterialResource::_async_load() {
    // Load self first
    if (!_async_load_from_file()) {
        co_return;
    }
    
    // Wait for all dependent resources
    for (auto &dep : _depended_resources) {
        if (dep) {
            co_await dep->await_loading();
        }
    }
    
    unsafe_set_loaded();
    co_return;
}
```

### 5. Device Installation (`_install`)

Called in render thread to upload data to GPU:

```cpp
bool MyResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    
    // Must be called in render thread
    if (!RenderDevice::is_rendering_thread()) [[unlikely]] {
        LUISA_ERROR("_install can only be called in render-thread.");
    }
    
    std::lock_guard lck{_async_mtx};
    
    // Check if already installed or not ready
    if (!_device_res) return false;
    if (_device_res->is_uploaded()) return false;
    
    // Upload to GPU
    return _device_res->upload(render_device->lc_main_cmd_list());
}
```

### 6. Saving (`unsafe_save_to_path`)

```cpp
bool MyResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    
    // Validate state
    if (!_device_res || _device_res->host_data().empty()) {
        return false;
    }
    
    // Write to file
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_device_res->host_data());
    return true;
}
```

### 7. Metadata Serialization

Metadata is stored in `.meta` files and loaded before the resource data:

```cpp
void MyResource::serialize_meta(ObjSerialize const &obj) const {
    std::shared_lock lck{_async_mtx};
    obj.ar.value(_vertex_count, "vertex_count");
    obj.ar.value(_triangle_count, "triangle_count");
    
    // Serialize arrays
    obj.ar.start_array();
    for (auto &offset : _submesh_offsets) {
        obj.ar.value(offset);
    }
    obj.ar.end_array("submesh_offsets");
}

void MyResource::deserialize_meta(ObjDeSerialize const &obj) {
    std::lock_guard lck{_async_mtx};
    
    obj.ar.value(_vertex_count, "vertex_count");
    obj.ar.value(_triangle_count, "triangle_count");
    
    uint64_t size;
    if (obj.ar.start_array(size, "submesh_offsets")) {
        _submesh_offsets.reserve(size);
        for (auto i : vstd::range(size)) {
            uint v;
            if (obj.ar.value(v)) {
                _submesh_offsets.push_back(v);
            }
        }
        obj.ar.end_scope();
    }
}
```

### 8. Factory Methods

Provide factory methods for creating resources programmatically:

```cpp
void MyResource::create_empty(uint64_t size_bytes, bool create_device_buffer) {
    // Wait for any ongoing loading
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Resource loading.");
    }
    
    // Reset loading status
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);
    
    std::lock_guard lck{_async_mtx};
    _device_res.reset();
    _device_res = new DeviceResource{};
    _size_bytes = size_bytes;
    _create_device_buffer = create_device_buffer;
    
    if (size_bytes > 0) {
        _device_res->_host_data.push_back_uninitialized(size_bytes);
    }
    
    // Mark as loaded
    unsafe_set_loaded();
}
```

### 9. Resource Registration

End the `.cpp` file with the registration macro:

```cpp
DECLARE_WORLD_OBJECT_REGISTER(MyResource)
```

This registers the type with the object pool system, enabling:
- `create_object<MyResource>()`
- `get_resource<MyResource>(guid)`
- Proper memory management via object pools

## Common Resource Types Examples

### Simple Buffer Resource

Like `BufferResource` - stores raw bytes:

```cpp
struct BufferResource final : ResourceBaseImpl<BufferResource> {
    DECLARE_WORLD_OBJECT_FRIEND(BufferResource)
    using BaseType = ResourceBaseImpl<BufferResource>;

private:
    RC<DeviceBuffer> _device_buffer;
    uint64_t _size_bytes{};
    mutable rbc::shared_atomic_mutex _async_mtx;
    
    BufferResource();
    ~BufferResource();

public:
    bool empty() const;
    [[nodiscard]] auto size_bytes() const { return _size_bytes; }
    [[nodiscard]] luisa::vector<std::byte> *host_data();
    [[nodiscard]] DeviceBuffer *device_buffer() const;
    void create_empty(uint64_t size_bytes, bool create_device_buffer);

    rbc::coroutine _async_load() override;
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;

protected:
    bool _install() override { return false; }
    bool unsafe_save_to_path() const override;
};
```

### Complex Resource with Dependencies

Like `MaterialResource` - references other resources:

```cpp
struct MaterialResource final : ResourceBaseImpl<MaterialResource> {
    DECLARE_WORLD_OBJECT_FRIEND(MaterialResource)
    using BaseType = ResourceBaseImpl<MaterialResource>;

private:
    mutable rbc::shared_atomic_mutex _async_mtx;
    luisa::vector<RC<Resource>> _depended_resources;  // Dependencies
    MaterialStub::MatDataType _mat_data;
    bool _loaded : 1 {false};
    bool _dirty : 1 {true};
    
    MaterialResource();
    ~MaterialResource();

public:
    void load_from_json(luisa::string_view json);
    luisa::BinaryBlob write_content_to();
    
    rbc::coroutine _async_load() override;

protected:
    bool _install() override;
    bool unsafe_save_to_path() const override;
    void _load_from_json(luisa::string_view json, bool set_to_loaded);
};
```

## Thread Safety Best Practices

1. **Always use `_async_mtx`** for protecting resource state
2. **Use `std::shared_lock`** for read operations
3. **Use `std::lock_guard`** for write operations
4. **Keep locks short** - don't hold locks across I/O operations
5. **Check loading status** before accessing data

## File Organization

- Header: `rbc/runtime/include/rbc_world/resources/my_resource.h`
- Implementation: `rbc/runtime/src/world/resources/my_resource.cpp`
- Binary data: `{binary_root}/{guid}`
- Metadata: `{meta_root}/{guid}.meta`

## Testing Resources

Resources can be tested by:

1. Creating via `create_object<MyResource>()`
2. Setting path and calling `load()`
3. Using `await_loading()` in coroutines
4. Checking `loaded()` and `installed()` states

```cpp
auto res = create_object<MyResource>();
res->load();  // Starts async loading
res->wait_loading();  // Blocks until loaded
assert(res->loaded());
```
