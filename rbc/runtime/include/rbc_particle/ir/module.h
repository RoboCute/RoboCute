#pragma once

#include <luisa/luisa-compute.h>

namespace rbc::ps {

class IRType;
class Symbol;
class Instruction;

struct NodeRef {
    size_t _0;
    bool operator==(const NodeRef &other) const {
        return _0 == other._0;
    }
};

class IRModule {
public:
    enum class Tag {
        Block,
        Function,
        Kernel,
    };
    Tag m_tag;
    luisa::string m_identifier;

    eastl::vector<eastl::unique_ptr<IRType>> m_types;
    eastl::vector<eastl::unique_ptr<Symbol>> m_symbols;
    eastl::vector<eastl::unique_ptr<Instruction>> m_instructions;

    eastl::vector<size_t> m_param_ids;
    // local
    eastl::vector<size_t> m_local_ids;
    eastl::vector<size_t> m_builtin_ids;
    eastl::vector<size_t> m_const_ids;
    eastl::vector<size_t> m_temp_ids;

public:
    explicit IRModule(
        Tag tag, luisa::string identifier, eastl::vector<eastl::unique_ptr<IRType>> types, eastl::vector<eastl::unique_ptr<Symbol>> symbols, eastl::vector<eastl::unique_ptr<Instruction>> m_instructions) noexcept;

    ~IRModule();
    // delete copy
    IRModule(const IRModule &) = delete;
    IRModule &operator=(const IRModule &) = delete;
    // default move
    IRModule(IRModule &&);
    IRModule &operator=(IRModule &&) = delete;

    luisa::span<const luisa::unique_ptr<Symbol>> symbols() const noexcept { return m_symbols; }
    [[nodiscard]] luisa::string identifier() const noexcept { return m_identifier; }
    luisa::span<const luisa::unique_ptr<Instruction>> instructions() const noexcept { return m_instructions; }

    luisa::span<const size_t> param_ids() const noexcept { return m_param_ids; }
    luisa::span<const size_t> local_ids() const noexcept { return m_local_ids; }
    luisa::span<const size_t> builtin_ids() const noexcept { return m_builtin_ids; }
    luisa::span<const size_t> const_ids() const noexcept { return m_const_ids; }
    luisa::span<const size_t> temp_ids() const noexcept { return m_temp_ids; }

private:
public:
    // for debugging
    [[nodiscard]] luisa::string dump() const noexcept;
};

}// namespace rbc::ps