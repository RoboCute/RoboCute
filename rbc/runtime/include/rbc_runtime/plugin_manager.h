#pragma once
#include <luisa/core/stl/vector.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/pool.h>
#include <luisa/core/fiber.h>
#include <rbc_runtime/plugin_module.h>
namespace rbc {
struct PluginModule;
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
    void execute(
        vstd::function<void(PluginModule *)> const &callback) const;
    void execute_parallel(
        vstd::function<void(PluginModule *)> &&callback) const;
};
struct RBC_RUNTIME_API PluginManager {
private:
    vstd::HashMap<
        luisa::string,
        RC<PluginModule>>
        loaded_modules;
public:
    PluginModule *load_module(luisa::string_view name);

    CompiledGraph compile() const;
};
}// namespace rbc