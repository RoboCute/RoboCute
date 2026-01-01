/**
 * @file symbol.cpp
 * @brief The PIR Symbol Implementation
 * @author sailing-innocent
 * @date 2024-05-30
 */

#include "rbc_particle/ir/symbol.h"
#include "rbc_particle/ir/literal.h"
#include "rbc_particle/ir/type.h"

namespace rbc::ps {

Symbol::Symbol(Tag tag, const IRType *type, int arr_len, const Symbol *parent, luisa::string identifier, eastl::vector<IRLiteral> initial_values) noexcept
    : m_tag{tag}, m_arr_len{arr_len}, m_type{type}, m_parent{parent}, m_identifier{std::move(identifier)}, m_initial_values{std::move(initial_values)} {
}
Symbol::Symbol(Symbol &&) noexcept {
    // move init value
    m_initial_values = std::move(m_initial_values);
}
Symbol::~Symbol() noexcept {
    // deallocate initial values
    for (auto &value : m_initial_values) {
        value.~IRLiteral();
    }
}

[[nodiscard]] luisa::string_view Symbol::dump(Tag tag) noexcept {
    switch (tag) {
        case Tag::SYM_PARAM:
            return "SYM_PARAM";
        case Tag::SYM_LOCAL:
            return "SYM_LOCAL";
        case Tag::SYM_TEMP:
            return "SYM_TEMP";
        case Tag::SYM_CONST:
            return "SYM_CONST";
        case Tag::SYM_BUILTIN:
            return "SYM_BUILTIN";
        default:
            return "UNKNOWN";
    }
}

luisa::string Symbol::dump() const noexcept {
    luisa::string s = "";
    s += "Symbol: ";
    s += dump(m_tag);
    s += " ";
    s += m_type->dump();
    s += " ";
    s += m_identifier;
    s += "\n";
    return s;
}

eastl::span<const IRLiteral> Symbol::initial_values() const noexcept {
    return m_initial_values;
}

}// namespace rbc::ps