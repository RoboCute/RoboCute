#pragma once
#include <rbc_runtime/plugin_manager.h>
#include <rbc_runtime/plugin_module.h>
namespace rbc {
PluginModule::PluginModule() : _node_pool(256, false) {}
void PluginModule::deref() {
    for (size_t idx = 0; idx < _allocated_nodes.size();) {
        auto node = _allocated_nodes[idx];
        if (node->self.is_type_of<RCWeak<PluginModule>>()) {
            _remove_node(node);
            continue;
        }
        ++idx;
    }
}
PluginModule::~PluginModule() {
    for (auto node : _allocated_nodes) {
        if (node->left) {
            node->left->right = node->right;
        }
        if (node->right) {
            node->right->left = node->left;
        }
        _node_pool.destroy(node);
    }
    _allocated_nodes.clear();
}
auto PluginModule::_create_node(bool is_weak) -> Node * {
    auto r = _node_pool.create();
    if (is_weak) {
        r->self.reset_as<RCWeak<PluginModule>>(this);
    } else {
        r->self.reset_as<RC<PluginModule>>(this);
    }
    r->allocated_idx = _allocated_nodes.size();
    _allocated_nodes.emplace_back(r);
    return r;
}
void PluginModule::_remove_node(Node *node) {
    if (node->left) {
        node->left->right = node->right;
    }
    if (node->right) {
        node->right->left = node->left;
    }
    if (node->allocated_idx < _allocated_nodes.size() - 1) {
        auto v = _allocated_nodes.back();
        _allocated_nodes[node->allocated_idx] = v;
        v->allocated_idx = node->allocated_idx;
    }
    _allocated_nodes.pop_back();
    _node_pool.destroy(node);
}
void PluginModule::add_depend(PluginModule *before, PluginModule *after) {
    auto before_node = before->_create_node(false);
    auto after_node = after->_create_node(true);
    after_node->right = before->_after_nodes;
    before->_after_nodes = after_node;
    before_node->right = after->_before_nodes;
    after->_before_nodes = before_node;
}
}// namespace rbc