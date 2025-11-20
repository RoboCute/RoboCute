#pragma once
#include <luisa/core/shared_function.h>
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

PluginManager::CompiledGraph::CompiledGraph() : pool(256, false) {}
PluginManager::CompiledGraph::~CompiledGraph() {
    std::destroy(allocated_nodes.begin(), allocated_nodes.end());
}

auto PluginManager::compile() const -> CompiledGraph {
    vstd::HashMap<PluginModule *, CompiledNode *> node_map;
    CompiledGraph graph;
    auto create_node = [&](PluginModule *module) -> CompiledNode * {
        if (!module) return nullptr;
        return node_map.try_emplace(module, vstd::lazy_eval([&]() {
                                        auto ptr = graph.pool.create();
                                        graph.allocated_nodes.emplace_back(ptr);
                                        ptr->ptr = module;
                                        return ptr;
                                    }))
            .first.value();
    };
    // init nodes
    for (auto &i : loaded_modules) {
        if (i->_before_nodes == nullptr) {
            auto n = create_node(i.get());
            auto after = n->ptr->_after_nodes;
            while (after) {
                auto depended_node = create_node(after->self.visit_or((PluginModule *)nullptr, []<typename T>(T const &t) {
                    if constexpr (std::is_same_v<T, RC<PluginModule>>) {
                        return t.get();
                    } else {
                        return t.lock().get();
                    }
                }));
                if (depended_node) {
                    n->after_self.emplace_back(depended_node);
                    depended_node->depended_count += 1;
                }
                after = after->right;
            }
        }
    }
    // get root
    for (auto &i : node_map) {
        if (i.second->depended_count == 0) {
            graph.root_nodes.emplace_back(i.second);
        }
    }
    return graph;
}
void PluginManager::execute_parallel(
    CompiledGraph &graph,
    vstd::function<void(PluginModule *)> &&callback) const {
    luisa::vector<CompiledNode *> stack;
    luisa::fiber::counter all_counter(graph.allocated_nodes.size());
    // init
    for (auto &i : graph.allocated_nodes) {
        i->lefted_count = i->depended_count;
        i->depended_counter.add(i->depended_count);
    }
    vstd::push_back_all(stack, luisa::span{graph.root_nodes});
    // execute
    luisa::SharedFunction<void(PluginModule *)> shared_func{std::move(callback)};
    while (!stack.empty()) {
        auto v = stack.back();
        stack.pop_back();
        luisa::fiber::schedule([v, shared_func, all_counter]() {
            v->depended_counter.wait();
            shared_func(v->ptr);
            for (auto &aft : v->after_self) {
                aft->depended_counter.done();
            }
            all_counter.done();
        });
        for (auto &aft : v->after_self) {
            if (--aft->lefted_count == 0) {
                stack.push_back(aft);
            }
        }
    }
    all_counter.wait();
}
void PluginManager::execute(
    CompiledGraph &graph,
    vstd::function<void(PluginModule *)> const &callback) const {
    luisa::vector<CompiledNode *> stack;
    // init
    for (auto &i : graph.allocated_nodes) {
        i->lefted_count = i->depended_count;
    }
    vstd::push_back_all(stack, luisa::span{graph.root_nodes});
    // execute
    while (!stack.empty()) {
        auto v = stack.back();
        stack.pop_back();
        callback(v->ptr);
        for (auto &aft : v->after_self) {
            if (--aft->lefted_count == 0) {
                stack.push_back(aft);
            }
        }
    }
}
}// namespace rbc