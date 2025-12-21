# RBCCore

一些底层功能

- utils
  - binary_search
  - curve
  - float-Pack
  - mathematics_double
- atomic
- binary_file_writer
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