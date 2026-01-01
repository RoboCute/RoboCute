#pragma once
#include <luisa/luisa-compute.h>

namespace rbc {

class IRModule;
class Instruction;

class IRFuncBuilder {
    using FuncBuilder = luisa::compute::detail::FunctionBuilder;
    using Expression = luisa::compute::Expression;
    // maps
    luisa::unordered_map<luisa::string, const Expression *> m_expr_map;

public:
    IRFuncBuilder() = default;
    // delte copy and move
    IRFuncBuilder(const IRFuncBuilder &) = delete;
    IRFuncBuilder(IRFuncBuilder &&) = delete;
    IRFuncBuilder &operator=(const IRFuncBuilder &) = delete;
    IRFuncBuilder &operator=(IRFuncBuilder &&) = delete;

private:
    // internal helper
    const Expression *_find_ref(const luisa::string_view name) const noexcept;

    void _built_in_idx(FuncBuilder &cur) noexcept;
    void _mul(const Instruction *inst, FuncBuilder &cur) noexcept;
    void _add(const Instruction *inst, FuncBuilder &cur) noexcept;
    void _sub(const Instruction *inst, FuncBuilder &cur) noexcept;
    void _read(const Instruction *inst, FuncBuilder &cur) noexcept;
    void _write(const Instruction *inst, FuncBuilder &cur) noexcept;

public:
    luisa::shared_ptr<const FuncBuilder> build_func(const IRModule &module) noexcept;
};

}// namespace rbc