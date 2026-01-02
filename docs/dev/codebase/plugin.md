# Robocute Plugin

Robocute从开发的阶段就采用插件式的开发，所有的render, world, editor均以插件的形式编译和加载，使用`PluginManager`统一管理，如下所示加载editor模块

```cpp
#include <rbc_plugin/plugin_manager.h>
int main(int argc, char *argv[]) {
    rbc::PluginManager::init();
    auto defer_destroy = vstd::scope_exit([&] {
        rbc::PluginManager::destroy_instance();
    });
    auto rbc_editor_module = rbc::PluginManager::instance().load_module("rbc_editor_module");
    auto value = rbc_editor_module->invoke<int(int, char *[])>(
        "dll_main",
        argc,
        argv);
    return value;
}
```

这样的开发范式为未来的python嵌入和插件拓展提供了便利。