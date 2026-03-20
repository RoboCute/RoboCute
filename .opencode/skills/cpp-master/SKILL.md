---
name: cpp-master
description: C++ coding guidelines and best practices for RoboCute project
triggers:
  - file_types: [".cpp", ".hpp", ".h", ".cc", ".cxx"]
---

# C++ Master Skill for RoboCute

This skill provides comprehensive C++ coding guidelines for the RoboCute project.

## Naming Conventions

- **Class names**: Use `CamelCase` (e.g., `MyClass`, `RenderPipeline`)
- **Functions and public variables**: Use `snake_case` (e.g., `get_value`, `process_data`)
- **Private/protected member variables and functions**: Use `_snake_case` (prefix with underscore)
  - Example: `_private_var`, `_internal_helper()`
- **Constants**: Use `kCamelCase` or `UPPER_SNAKE_CASE` for macros
- **Template parameters**: Use `CamelCase`
- **Condition Predict** add [[unlikely]] for log warning or error scope.

## Code Style

- Use **4 spaces** for indentation (no tabs)
- Maximum line length: **100 characters**
- Always use braces `{}` for control structures, even for single-line blocks
- Place opening braces on the same line (K&R style)

```cpp
// Good
if (condition) {
    do_something();
}

// Bad
if (condition)
    do_something();
```

## Project-Specific Types

### Reference Counting (RC) System

The project uses a custom reference counting system for memory management:

```cpp
#include <rbc_core/rc.h>

// Creating a reference-counted object
class MyClass : public rbc::RCBase {
public:
    void do_something() {}
};

// Usage
rbc::RC<MyClass> obj = rbc::RC<MyClass>::New();
obj->do_something();

// Weak references
rbc::RCWeak<MyClass> weak_ref = obj;
auto locked = weak_ref.lock();
if (locked) {
    locked->do_something();
}
```

### vstd Containers

Use the project's custom container library (`vstd`) instead of STL where available:

```cpp
#include <luisa/vstl/common.h>

// HashMap
vstd::HashMap<int, std::string> map;
map.emplace(1, "value");
auto iter = map.find(1);
if (iter) {
    // Key exists
    std::string value = iter.value();
}

// Optional
luisa::optional<int> opt = 42;
if (opt.has_value()) {
    int val = opt.value();
}

// Variant
luisa::variant<int, float, std::string> var = "hello";
```

### String Types

Use project-specific string types:

```cpp
#include <luisa/core/stl/string.h>

luisa::string str = "project string";  // Uses project's allocator
```

## Memory Management

- Use `rbc::RC<T>` for reference-counted objects
- Use `luisa::new_with_allocator<T>()` for allocator-aware allocation
- Use `luisa::delete_with_allocator<T>()` for deallocation
- Raw pointers should be non-owning; ownership is managed by RC

```cpp
// Good - reference counted
rbc::RC<MyClass> obj = rbc::RC<MyClass>::New(args);

// Good - non-owning raw pointer
MyClass* ptr = obj.get();

// Bad - manual new/delete
MyClass* obj = new MyClass();  // Avoid this
delete obj;                     // Avoid this
```

## Class Design

### Inheritance

```cpp
// Base class with virtual destructor
class Base : public rbc::RCBase {
public:
    virtual ~Base() = default;
    virtual void virtual_method() = 0;
};

// Derived class
class Derived : public Base {
public:
    void virtual_method() override {}
    
private:
    int _private_member = 0;
};
```

### Interface Pattern

```cpp
// Interface with pure virtual methods
class IMyInterface : public rbc::RCBase {
public:
    virtual void do_work() = 0;
    virtual int get_value() const = 0;
};

// Implementation
class MyImplementation : public IMyInterface {
public:
    void do_work() override {
        // Implementation
    }
    
    int get_value() const override {
        return _value;
    }
    
private:
    int _value = 0;
};
```

## Build System (Xmake)

This project uses **xmake** as the build system.

### Common Commands

```bash
# First-time setup (debug mode)
xmake f -m debug -c

# Build all targets
xmake

# Build specific target
xmake <target_name>

# Run a target
xmake run <target_name>

# List all targets
xmake -l

# Clean build artifacts
xmake clean
```

### Target Configuration

```lua
-- xmake.lua example
target("my_module")
    add_rules('lc_basic_settings', {
        project_kind = 'shared'  -- 'shared', 'binary', or 'static'
    })
    add_files('src/**.cpp')
    add_includedirs('include')
target_end()
```

## Headers

### Include Order

1. Corresponding header for .cpp file (if any)
2. Project headers
3. Third-party headers
4. Standard library headers

```cpp
// my_class.cpp
#include "my_class.h"           // 1. Corresponding header

#include <rbc_core/memory.h>    // 2. Project headers
#include <luisa/vstl/common.h>

#include <third_party/lib.h>    // 3. Third-party

#include <vector>               // 4. Standard library
#include <string>
```

### Header Guards

Always use `#pragma once`:

```cpp
#pragma once

// Header content
```

## Error Handling

- Use `luisa::optional<T>` for values that may not exist
- Use early returns to reduce nesting
- Use assertions for programming errors: `LUISA_DEBUG_ASSERT(condition, "message")`

```cpp
// Good - early return
rbc::RC<MyClass> get_object(int id) {
    auto iter = _objects.find(id);
    if (!iter) {
        return nullptr;
    }
    return iter.value();
}

// Good - optional for nullable values
luisa::optional<int> maybe_get_value() {
    if (condition) {
        return 42;
    }
    return luisa::nullopt;
}
```

## Thread Safety

- Use `std::atomic` for atomic operations
- Use `luisa::spin_mutex` for lightweight locking
- Use `std::shared_mutex` for read-heavy scenarios
- Use `rbc::shared_atomic_mutex` for read-heavy scenarios, lightweight locking

```cpp
#include <luisa/core/spin_mutex.h>

class ThreadSafeClass {
public:
    void set_value(int v) {
        std::lock_guard<luisa::spin_mutex> lock(_mutex);
        _value = v;
    }
    
    int get_value() const {
        std::lock_guard<luisa::spin_mutex> lock(_mutex);
        return _value;
    }
    
private:
    mutable luisa::spin_mutex _mutex;
    int _value = 0;
};
```

## Modern C++ Features

- Use C++20 features (designated initializers, concepts, etc.)
- Use `auto` for type deduction when it improves readability
- Use range-based for loops
- Use structured bindings

```cpp
// Good - range-based for
for (const auto& item : container) {
    process(item);
}

// Good - structured bindings
auto [inserted, iter] = map.try_emplace(key, value);

// Good - concepts (C++20)
template<typename T>
    requires std::is_integral_v<T>
T add(T a, T b) {
    return a + b;
}
```

## Testing

Tests are located in `rbc/tests/` using doctest framework:

```cpp
#include "../_framework/test_util.h"

TEST_SUITE("MyFeature") {
    TEST_CASE("basic_functionality") {
        MyClass obj;
        CHECK(obj.get_value() == 42);
        CHECK(obj.process() == true);
    }
    
    TEST_CASE("edge_cases") {
        // Test edge cases
    }
}
```

## API Export

Mark public API classes/functions with `RBC_CORE_API` or module-specific export macros:

```cpp
// In rbc/core/include/rbc_config.h or similar
#define RBC_CORE_API __declspec(dllexport)  // Windows
// or
#define RBC_CORE_API __attribute__((visibility("default")))  // Linux/macOS

// Usage
struct RBC_CORE_API MyPublicClass {
    void public_method();
};
```

## Best Practices

1. **Prefer composition over inheritance**
2. **Keep functions small and focused** (single responsibility)
3. **Minimize public interface** - make members private by default
4. **Use const correctness** - mark methods and parameters const when possible
5. **Avoid raw loops** - use algorithms or range-based for
6. **Document public APIs** with clear comments
7. **Use strong types** - avoid primitive obsession
8. **Handle errors gracefully** - don't ignore error cases

## Common Patterns

### Factory Pattern

```cpp
class IShape : public rbc::RCBase {
public:
    virtual void draw() = 0;
    virtual ~IShape() = default;
};

class ShapeFactory {
public:
    static rbc::RC<IShape> create_circle(float radius);
    static rbc::RC<IShape> create_rectangle(float w, float h);
};
```

### Observer Pattern

```cpp
class IObserver : public rbc::RCBase {
public:
    virtual void on_event(const Event& e) = 0;
};

class Subject {
public:
    void add_observer(rbc::RC<IObserver> observer);
    void notify(const Event& e);
    
private:
    luisa::vector<rbc::RCWeak<IObserver>> _observers;
};
```

### RAII Pattern

```cpp
class FileHandle {
public:
    explicit FileHandle(const char* path) 
        : _file(fopen(path, "r")) {}
    
    ~FileHandle() {
        if (_file) {
            fclose(_file);
        }
    }
    
    // Disable copy, enable move
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept : _file(other._file) {
        other._file = nullptr;
    }
    
private:
    FILE* _file = nullptr;
};
```
