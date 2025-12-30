# RBCCore

一些底层功能

- utils
  - binary_search
  - curve
  - float-Pack
  - mathematics_double
- atomic
- binary_file_writer
- containers
  - concurrent_queue - 多生产者多消费者无锁队列
- func_serializer
- hash
- heap_object
- json_serde
- quaternion
- rc
- serde
- shared_atomic_mutex
- state_map
- transform
- type_info
  - md5 - string

## RTTI

Runtime Type Identification 运行时类型识别，C++中自带的一个简单宏，可以在运行时获取类型名称和信息。RTTI整体是静态注册的，当代码生成偏特化模板之后，不需要运行时做任何额外工作，类型就已经嵌入在代码中了。

`RTTI(ClassName)`

宏会注册以下方法
- `struct is_rtti_type<ClassName>`
  - `value`: true
  - `name`: ClassName

IRTTRBasic
- new
- StaticType
- rttr_get_type
- rttr_get_typeid
- rttr_serde_read
- rttr_serde_write
- rttr_cast
  - get_from_type
  - reinterpret_cast
  - `const_cast<IRTTRBasic*>(this)->rttr_cast<TO>()`
- rttr_is


## RuntimeStatic

RuntimeStatic的作用是用一个统一的入口`RuntimeStaticBase::init_all()`和出口`RuntimeStaticBase::dispose_all()`来初始化和释放无法显示声明的初始化和析构。比如RTTR中，对于RTTR类型的注册过程是由代码生成实现的，正式代码中无法统计到底有多少RTTR类型需要析构，这时候就需要RuntimeStatic来帮助自动注册。

RuntimeStatic是通过一个静态全局链表和Optional协作实现的

## RTTR 

- 2025-12-21 似乎暂时不太需要，rbc的需求是比较封闭的，直接switch即可

Runtime Type Reflection 运行时类型反射

C++当前反射还没有支持完全，大部分的反射都需要构建系统支持，在Robocute中我们采用python作为反射方案，通过Python实现一个RTTRBasic来和代码生成，来获得一些运行时的类型反射能力，这在资源系统和场景中非常有用


暂时不考虑虚继承\多基类的情况

record_data: class info
- comment
- file_name
- line
- name
- attrs
- name
- access
- is_virtual
- bases

### RTTRType

基础组件，记录了RTTRType的信息

- module
- type_category
- name
- name_space
- name_space_str
- full_name
- type_id
- size
- alignment
- each_name_space
- memory_traits_data
- is_primitive
- is_record
- is_enum
- build_primitive
- build_record
- build_enum
- is_based_on
- cast_to_base
- caster_to_base
- has_multiple_bases
- has_virtual_bases
- enum_underlying_type_id
- each_enum_items
- dtor_data
- dtor_invoker
- invoke_dtor
- each_bases
- each_ctor
- each_method
- each_field
- each_static_method
- each_static_field

### RTTRRecordData

- name 
- namespace
- type_id
- size
- alignment
- memory_traits_data

ExportMethodInvoker
```cpp
template <typename... Args>
struct ExportCtorInvoker<void(Args...)> {
  inline void invoke(void* p_obj, Args... args) const 
  {
    ASSERT(_ctor_data->native_invoke);
    auto invoker = reinterpret_cast<void(*)(void*, Args...)>(_ctor_data->native_invoke);
    invoker(p_obj, args...);
  }
}
```

```cpp
struct RTTRExportHelper {
  template<typename T, typename... Args>
  inline static void* export_ctor()
  {
    auto result = +[](void* p, Args... args) {
      new (p) T(std::forward<Args>(args)...);
    }
    return reinterpret_cast<void*>(result);
  }
};
```

### RTTRRecordBuilder

### IRTTRBasic

- placement new
- StaticType: `return ::skr::type_of<Decl>();`
- rttr_get_type
- rttr_get_type_id
- rttr_get_head_ptr: `return const_cast<void*>((const void*)this)`


### is_based_on

- RTTR最重要的一点就是在运行时依然保留类型的继承关系
- 分type_category讨论
- ERTTRTypeCategory::Primitive
- ERTTRTypeCategory::Record
    - _record_data.type_id
- RTTRTypeCategory::Enum

## RC

RefCounted

RBC_RC_INTERFACE
- rbc_rc_count
- rbc_rc_add_ref
- rbc_rc_weak_lock
- rbc_rc_release
- rbc_rc_weak_ref_count
- rbc_rc_weak_ref_counter
- rbc_rc_weak_ref_counter_notify_dead
- rbc_rc_delete

RBC_RC_IMPL

- RC
- RCWeak
- RCWeakLocker
- RCBase

## Memory Management

### 内存分配函数

RBC 提供了封装的内存分配函数，包括：
- `rbc_malloc` / `rbc_free` - 基本内存分配
- `rbc_malloc_aligned` / `rbc_free_aligned` - 对齐内存分配
- `rbc_calloc` / `rbc_calloc_aligned` - 零初始化内存分配
- `rbc_realloc` / `rbc_realloc_aligned` - 内存重分配
- `RBCNew<T>` / `RBCDelete<T>` - 类型安全的 new/delete
- `RBCNewAligned<T>` / `RBCDeleteAligned<T>` - 对齐的类型安全 new/delete

### 使用 AddressSanitizer (ASAN) 测试内存泄漏

AddressSanitizer (ASAN) 是一个快速的内存错误检测工具，可以检测：
- 内存泄漏 (memory leaks)
- 使用已释放内存 (use-after-free)
- 缓冲区溢出 (buffer overflow)
- 使用未初始化内存 (use of uninitialized memory)
- 内存对齐错误 (misaligned memory access)

#### 配置 ASAN

在 xmake 中启用 ASAN：

**Linux/macOS:**
```bash
# 配置 debug 模式并启用 ASAN
xmake f -m debug --sanitize=address

# 编译测试
xmake build test_core

# 运行测试（ASAN 会自动检测内存问题）
xmake run test_core
```

**Windows (使用 clang-cl):**
```bash
# 配置 debug 模式并启用 ASAN
xmake f -m debug --sanitize=address --toolchain=clang

# 编译测试
xmake build test_core

# 运行测试
xmake run test_core
```

#### 在代码中启用 ASAN

如果需要在特定 target 中启用 ASAN，可以在 `xmake.lua` 中添加：

```lua
target("test_core")
    -- 启用 AddressSanitizer
    if is_plat("linux", "macosx") then
        add_cxflags("-fsanitize=address")
        add_mxflags("-fsanitize=address")
        add_ldflags("-fsanitize=address")
    elseif is_plat("windows") and is_toolchain("clang") then
        add_cxflags("-fsanitize=address")
        add_mxflags("-fsanitize=address")
        add_ldflags("-fsanitize=address")
    end
```

#### 测试内存泄漏示例

编写测试用例来验证内存分配/释放是否正确：

```cpp
#include "test_util.h"
#include <rbc_core/memory.h>

TEST_SUITE("core") {
    TEST_CASE("memory_leak_detection") {
        // 正确使用：分配后释放
        void *ptr = rbc_malloc(100);
        CHECK(ptr != nullptr);
        rbc_free(ptr);  // ASAN 不会报告泄漏
        
        // 错误示例：分配后未释放（会被 ASAN 检测到）
        // void *leaked = rbc_malloc(100);
        // 如果忘记释放，ASAN 会报告：
        // "ERROR: LeakSanitizer: detected memory leaks"
    }
    
    TEST_CASE("rbc_new_delete_leak_detection") {
        // 测试 RBCNew/RBCDelete 是否正确配对
        int *ptr = RBCNew<int>(42);
        CHECK(ptr != nullptr);
        CHECK(*ptr == 42);
        RBCDelete(ptr);  // 正确释放，ASAN 不会报告泄漏
        
        // 错误示例：忘记调用 RBCDelete
        // int *leaked = RBCNew<int>(42);
        // ASAN 会检测到泄漏
    }
    
    TEST_CASE("use_after_free_detection") {
        int *ptr = RBCNew<int>(100);
        RBCDelete(ptr);
        
        // 错误示例：使用已释放的内存（会被 ASAN 检测到）
        // *ptr = 200;  // ASAN 会报告：
        // "ERROR: AddressSanitizer: use-after-free"
    }
    
    TEST_CASE("buffer_overflow_detection") {
        int *arr = (int *)rbc_malloc(10 * sizeof(int));
        
        // 正确使用
        arr[9] = 100;
        
        // 错误示例：缓冲区溢出（会被 ASAN 检测到）
        // arr[10] = 200;  // ASAN 会报告：
        // "ERROR: AddressSanitizer: heap-buffer-overflow"
        
        rbc_free(arr);
    }
}
```

#### ASAN 输出解读

当 ASAN 检测到内存问题时，会输出类似以下的信息：

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 100 byte(s) in 1 object(s) allocated from:
    #0 0x7f8b2c0d4b40 in malloc
    #1 0x55a1b2c3d4e0 in rbc_malloc rbc/core/src/memory.cpp:112
    #2 0x55a1b2c3e1a0 in test_memory_leak test_memory.cpp:15

SUMMARY: AddressSanitizer: 100 byte(s) leaked in 1 allocation(s).
=================================================================
```

关键信息：
- **Direct leak**: 直接泄漏（未释放的内存）
- **Indirect leak**: 间接泄漏（通过指针间接持有的内存）
- **Stack trace**: 显示分配内存的调用栈，帮助定位问题

#### 环境变量配置

可以通过环境变量控制 ASAN 的行为：

```bash
# 设置 ASAN 选项
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1"

# Windows (PowerShell)
$env:ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1"

# 运行测试
xmake run test_core
```

常用 ASAN 选项：
- `detect_leaks=1` - 启用内存泄漏检测（默认开启）
- `halt_on_error=1` - 检测到错误时停止程序
- `abort_on_error=1` - 检测到错误时中止程序
- `print_stats=1` - 打印统计信息
- `check_initialization_order=1` - 检查初始化顺序问题
- `detect_stack_use_after_return=1` - 检测栈返回后使用

#### 注意事项

1. **性能影响**: ASAN 会显著降低程序运行速度（约 2-3 倍），仅用于调试
2. **内存占用**: ASAN 会增加内存使用（约 2-3 倍）
3. **Windows 限制**: Windows 上需要 clang-cl 工具链，MSVC 不支持 ASAN
4. **与 mimalloc 兼容性**: 如果使用 mimalloc，可能需要特殊配置
5. **测试环境**: 建议在 CI/CD 中定期运行 ASAN 测试

#### 最佳实践

1. **定期运行**: 在每次代码提交前运行 ASAN 测试
2. **完整覆盖**: 确保所有内存分配函数都有对应的测试用例
3. **边界测试**: 测试边界情况（零大小、大内存、对齐等）
4. **配对检查**: 确保每个 `malloc` 都有对应的 `free`，每个 `RBCNew` 都有对应的 `RBCDelete`
5. **文档记录**: 在代码注释中说明内存所有权和生命周期

## ConcurrentQueue

RBC 提供了一个基于 moodycamel 实现的多生产者、多消费者无锁队列（lock-free queue）。该队列使用 RBC 的内存分配系统，确保所有内存操作都通过 RBC 的内存管理接口。

### 特性

- **无锁设计**: 使用原子操作实现，无需互斥锁
- **多生产者多消费者**: 支持多个线程同时进行入队和出队操作
- **高性能**: 针对高并发场景优化
- **内存管理**: 使用 RBC 的内存分配函数，与 RBC 内存系统集成
- **类型安全**: 模板化设计，支持任意类型

### 基本使用

```cpp
#include <rbc_core/containers/rbc_concurrent_queue.h>

// 创建队列
rbc::ConcurrentQueue<int> queue;

// 入队
queue.enqueue(42);
queue.enqueue(100);

// 出队
int value;
if (queue.try_dequeue(value)) {
    // 处理 value
}
```

### Producer Token 和 Consumer Token

为了提高性能，可以使用 producer token 和 consumer token：

```cpp
rbc::ConcurrentQueue<int> queue;

// 创建 producer token（每个线程一个）
rbc::ConcurrentQueue<int>::producer_token_t producer(queue);

// 使用 token 入队（性能更好）
queue.enqueue(producer, 42);

// 创建 consumer token（每个线程一个）
rbc::ConcurrentQueue<int>::consumer_token_t consumer(queue);

// 使用 token 出队（性能更好）
int value;
queue.try_dequeue(consumer, value);
```

### 批量操作

支持批量入队和出队，提高吞吐量：

```cpp
// 批量入队
std::vector<int> items = {1, 2, 3, 4, 5};
queue.enqueue_bulk(items.begin(), items.size());

// 批量出队
std::vector<int> results(5);
size_t dequeued = queue.try_dequeue_bulk(results.begin(), 5);
```

### 多线程示例

```cpp
rbc::ConcurrentQueue<int> queue;
const int num_producers = 4;
const int num_consumers = 2;

// 生产者线程
std::vector<std::thread> producers;
for (int i = 0; i < num_producers; ++i) {
    producers.emplace_back([&queue, i]() {
        for (int j = 0; j < 100; ++j) {
            queue.enqueue(i * 100 + j);
        }
    });
}

// 消费者线程
std::vector<std::thread> consumers;
for (int i = 0; i < num_consumers; ++i) {
    consumers.emplace_back([&queue]() {
        int value;
        while (queue.try_dequeue(value)) {
            // 处理 value
        }
    });
}

// 等待所有线程完成
for (auto &t : producers) t.join();
for (auto &t : consumers) t.join();
```

### API 参考

#### 构造和析构

- `ConcurrentQueue(size_t capacity = 32 * BLOCK_SIZE)` - 创建队列，可选指定初始容量
- `ConcurrentQueue(size_t minCapacity, size_t maxExplicitProducers, size_t maxImplicitProducers)` - 根据预期使用情况创建队列

#### 入队操作

- `bool enqueue(T const &item)` - 入队（复制）
- `bool enqueue(T &&item)` - 入队（移动）
- `bool enqueue(producer_token_t const &token, T const &item)` - 使用 token 入队
- `bool try_enqueue(T const &item)` - 尝试入队（不分配内存）
- `bool enqueue_bulk(It itemFirst, size_t count)` - 批量入队
- `bool enqueue_bulk(producer_token_t const &token, It itemFirst, size_t count)` - 使用 token 批量入队

#### 出队操作

- `bool try_dequeue(U &item)` - 尝试出队
- `bool try_dequeue(consumer_token_t &token, U &item)` - 使用 token 尝试出队
- `size_t try_dequeue_bulk(It itemFirst, size_t max)` - 批量出队
- `size_t try_dequeue_bulk(consumer_token_t &token, It itemFirst, size_t max)` - 使用 token 批量出队

#### 其他操作

- `size_t size_approx() const` - 获取队列大小的近似值
- `static constexpr bool is_lock_free()` - 检查底层原子操作是否无锁

### Traits 配置

`RBCConcurrentQueueTraits` 提供了以下可配置参数：

- `BLOCK_SIZE` - 块大小（默认 32，必须是 2 的幂）
- `EXPLICIT_INITIAL_INDEX_SIZE` - 显式生产者的初始索引大小（默认 32）
- `IMPLICIT_INITIAL_INDEX_SIZE` - 隐式生产者的初始索引大小（默认 32）
- `INITIAL_IMPLICIT_PRODUCER_HASH_SIZE` - 隐式生产者哈希表初始大小（默认 32）
- `RECYCLE_ALLOCATED_BLOCKS` - 是否回收动态分配的块（默认 false）

### 性能建议

1. **使用 Token**: 对于频繁操作的线程，使用 producer/consumer token 可以提高性能
2. **批量操作**: 当需要处理多个元素时，使用批量操作减少开销
3. **避免频繁查询大小**: `size_approx()` 是近似值，频繁调用可能影响性能
4. **合理设置容量**: 根据预期使用情况设置初始容量，减少动态分配

### 内存管理

ConcurrentQueue 使用 RBC 的内存分配函数（`rbc_malloc`/`rbc_free`），确保：
- 所有内存分配都通过 RBC 的内存管理系统
- 支持内存池和追踪
- 与 RBC 的其他组件一致的内存管理策略