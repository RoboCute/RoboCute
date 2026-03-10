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
