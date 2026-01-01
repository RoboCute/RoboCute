#include "rbc_particle/ir/module.h"
#include "rbc_particle/ir/type.h"
#include "rbc_particle/ir/symbol.h"
#include "rbc_particle/ir/instruction.h"

namespace rbc::ps {
IRModule::IRModule(Tag tag, luisa::string identifier, eastl::vector<eastl::unique_ptr<IRType>> types, eastl::vector<eastl::unique_ptr<Symbol>> symbols, eastl::vector<eastl::unique_ptr<Instruction>> m_instructions) noexcept
    : m_tag(tag), m_identifier(std::move(identifier)), m_types(std::move(types)), m_symbols(std::move(symbols)), m_instructions(std::move(m_instructions)) {
    auto symbol_size = m_symbols.size();
    // store param and builtin ids
    for (size_t i = 0; i < symbol_size; i++) {
        auto symbol = m_symbols[i].get();
        if (symbol->tag() == Symbol::Tag::SYM_PARAM) {
            m_param_ids.emplace_back(i);
        } else if (symbol->tag() == Symbol::Tag::SYM_LOCAL) {
            m_local_ids.emplace_back(i);
        } else if (symbol->tag() == Symbol::Tag::SYM_BUILTIN) {
            m_builtin_ids.emplace_back(i);
        } else if (symbol->tag() == Symbol::Tag::SYM_CONST) {
            m_const_ids.emplace_back(i);
        } else if (symbol->tag() == Symbol::Tag::SYM_TEMP) {
            m_temp_ids.emplace_back(i);
        }
    }
}

IRModule::~IRModule() {}
IRModule::IRModule(IRModule &&) {}
luisa::string IRModule::dump() const noexcept {
    luisa::string s = "";
    // dump symbols
    LUISA_INFO("dump {} symbols", m_symbols.size());

    for (const auto &symbol : m_symbols) {
        // LUISA_INFO("dump symbol: {}", symbol->dump());
        s += symbol->dump();
    }

    // dump instructions
    LUISA_INFO("dump {} instructions", m_instructions.size());
    for (const auto &instruction : m_instructions) {
        // LUISA_INFO("dump instruction: {}", instruction->dump());
        s += instruction->dump();
    }

    return s;
}
}// namespace rbc::ps