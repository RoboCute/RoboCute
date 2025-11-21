#pragma once
#include <luisa/vstl/common.h>
#include <luisa/vstl/pool.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_config.h>
#include <rbc_core/rc.h>
namespace rbc {
struct PluginManager;
struct RBC_RUNTIME_API PluginModule : RCBase {
    friend struct PluginManager;
    struct Node {
        Node *left{};
        Node *right{};
        vstd::variant<RC<PluginModule>, RCWeak<PluginModule>> self{};
        uint64_t allocated_idx{~0ull};
    };
    luisa::shared_ptr<luisa::DynamicModule> dll_module;

private:
    luisa::string _name;
    vstd::Pool<Node, true> _node_pool;
    Node *_before_nodes{nullptr};// before -> self
    Node *_after_nodes{nullptr}; // self -> after
    luisa::vector<Node *> _allocated_nodes;
    Node *_create_node(bool is_weak);
    void _remove_node(Node *node);

public:
    static void add_depend(PluginModule *before, PluginModule *after);
    static void add_depend(PluginModule &before, PluginModule &after) {
        add_depend(&before, &after);
    }
    PluginModule(
        luisa::string_view name
    );
    ~PluginModule();
    void deref();
    PluginModule(PluginModule const &) = delete;
    PluginModule(PluginModule &&) = delete;
};
}// namespace rbc