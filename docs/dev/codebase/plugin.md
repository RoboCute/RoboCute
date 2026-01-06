# Plugin

## What is plugin

Plugin 指的是运行时在代码中显式加载动态库，显式获取动态库中的C FFI的模块管理方式。这种工作方式的特点是手动控制动态库以及动态库内部的静态变量生命周期，方便热更新或按需加载等。

## Plugin defines in xmake

为了能够区分公开与私有的变量，以及支持 plugin 模式，

通过下面案例定义一个名为 my_plugin 的 target:

```lua
-- public properties, can be inherited by other 
local function my_plugin_interface()
    add_includedirs('plugin_include', {
        public = true -- this option 'public' MUST exists, ignored otherwise
    })
end
-- private properties, will never
local function my_plugin_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_files('**.cpp')
    add_includedirs('private_include', {
        public = true -- this option 'public' is useless, 'private_include' will never be inherited by other targets
    })
end
--  global function defined in xmake/interface_target.lua
interface_target(
    'my_plugin', -- target base-name
    my_plugin_interface,
    my_plugin_impl,

    true  -- if no_link is true, add_deps('my_plugin') will never link to shared-library (like my_plugin.dll)
    -- no_link only for plugin mode, otherwise set to false
)
    -- other interface properties 

    add_includedirs('interface_include', {  -- inherited by other targets, not to 'my_plugin' target
        public = true
    })
target_end()
```

此时假设有 dummy target 依赖 my_plugin:

```lua
target('dummy')
    add_deps('my_plugin')
    add_files('dummy.cpp')
target_end()
```

## Plugin usage

那么在 dummy.cpp 中:

```cpp
#include <plugin_include/my_plugin.h> // good
#include <interface_include/my_plugin.h> // good
#include <private_include/my_plugin_private.h> // error: private_include not inherited.


void test(){
    MyPlugin::c_ffi_function(); // error: not linked

    luisa::DynamicModule my_plugin{"my_plugin"}; // good, explicit load 
    my_plugin.invoke<void()>("c_ffi_function"); // good, explicit invoke
}

```

## Circle depend

多个plugin可能产生环形依赖，可以使用 add_interface_depend 提供弱引用（只依赖属性而非target本身的构建顺序）：

```lua
-- foo
local function foo_interface() end
local function foo_impl() 
    add_deps('bar')         -- error: circle depend
    -- defined in xmake/interface_target.lua
    add_interface_depend('bar') -- good, only depend on public properties instead of bar's build 
end
interface_target('foo', foo_interface, foo_impl, true)

-- bar
local function bar_interface() end
local function bar_impl() 
    -- add_deps('foo')  -- may error if foo depend on bar
    add_interface_depend('foo') -- good
    
end
interface_target('bar', bar_interface, bar_impl, true)
```