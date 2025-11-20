#pragma once
#include <luisa/core/stl/vector.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/pool.h>
#include <luisa/core/fiber.h>
#include <rbc_runtime/plugin_module.h>
namespace rbc {
struct PluginModule;
struct RBC_RUNTIME_API PluginManager {
    luisa::vector<RC<PluginModule>> loaded_modules;
    struct CompiledNode {
        PluginModule *ptr{};
        luisa::vector<CompiledNode *> after_self;
        luisa::fiber::counter depended_counter;
        uint64_t depended_count{};
        uint64_t lefted_count{};
    };
    struct RBC_RUNTIME_API CompiledGraph {
        vstd::Pool<CompiledNode, true> pool;
        luisa::vector<CompiledNode *> allocated_nodes;
        luisa::vector<CompiledNode *> root_nodes;
        CompiledGraph();
        CompiledGraph(CompiledGraph &&) = default;
        ~CompiledGraph();
    };
    CompiledGraph compile() const;
    void execute(
        CompiledGraph &graph,
        vstd::function<void(PluginModule *)> const &callback) const;
    void execute_parallel(
        CompiledGraph &graph,
        vstd::function<void(PluginModule *)> &&callback) const;
};
}// namespace rbc