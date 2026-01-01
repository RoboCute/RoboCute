/**
 * @file parser.cpp
 * @brief Parser Implementation
 * @author sailing-innocent
 * @date 2024-05-29
 */

#include <luisa/luisa-compute.h>
#include <EASTL/vector.h>
#include <fstream>

#include "rbc_particle/ir/parser.h"
#include "rbc_particle/ir/literal.h"
#include "rbc_particle/ir/type.h"
#include "rbc_particle/ir/symbol.h"
#include "rbc_particle/ir/instruction.h"
#include "rbc_particle/ir/module.h"

namespace rbc {
namespace ps {
namespace detail {

[[nodiscard]] inline auto is_identifier_head(char c) noexcept {
    return isalpha(c) || c == '_' || c == '$';
}

[[nodiscard]] inline auto is_identifier_body(char c) noexcept {
    return isalnum(c) || c == '_' || c == '$' || c == '.';
}

[[nodiscard]] inline auto is_number_head(char c) noexcept {
    return isdigit(c) || c == '.' || c == '-' || c == '+';
}

[[nodiscard]] inline auto is_number_body(char c) noexcept {
    return isdigit(c) || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-';
}

}
}
}// namespace rbc::ps::detail

namespace rbc::ps {

IRParser::IRParser() {}
IRParser::~IRParser() {}
IRParser::IRParser(IRParser &&) {}

void IRParser::set_source(luisa::string source) noexcept {
    m_source = std::move(source);
    m_state.source = m_source;
    m_state.line = 0;
    m_state.column = 0;
}

void IRParser::load_file(luisa::string_view file_path) noexcept {
    std::ifstream file{luisa::filesystem::path{file_path}, std::ios::in | std::ios::app};
    if (!file.is_open()) {
        LUISA_ERROR("Failed to open file '{}'.", file_path);
    }
    luisa::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    set_source(source);
}

// construct the IRModule
eastl::unique_ptr<IRModule> IRParser::parse() noexcept {
    _parse_symbols();     // until decl symbol decl end
    _parse_instructions();// until instruction end
    LUISA_INFO("parse done, symbol size: {}, instruction size: {}", m_symbols.size(), m_instructions.size());
    return eastl::make_unique<IRModule>(
        IRModule::Tag::Kernel, "dummy", std::move(m_types), std::move(m_symbols), std::move(m_instructions));
}

}// namespace rbc::ps

// The Element-wise parser

namespace rbc::ps {

void IRParser::_restore(const State &state) noexcept { m_state = state; }

void IRParser::_skip_whitespaces() noexcept {
    auto is_whitespace = [](char c) noexcept {
        return c == ' ' || c == '\t' || c == '\f' || c == '\v' || c == '\r';
    };
    while (!_eof() && is_whitespace(_peek())) {
        static_cast<void>(_read());
    }
    // skip comments
    if (!_eof() && _peek() == '#') {
        while (!_eof() && !_eol()) {
            static_cast<void>(_read());
        }
    }
}

void IRParser::_skip_empty_lines() noexcept {
    while (!_eof()) {
        _skip_whitespaces();
        if (_eol()) {
            _match_eol();
        } else {
            break;
        }
    }
}

void IRParser::_parse_symbols() noexcept {
    // traverse the source to fetch the symbols
    while (!_eof()) {
        _skip_empty_lines();
        _skip_whitespaces();
        auto symbol = _parse_symbol();
        // not symbol, try instruction
        if (!symbol) {
            LUISA_INFO("not symbol");
            break;
        }

        // add the symbol to the symbol map
        m_symbol_map.emplace(symbol->identifier(), symbol.get());
        m_symbols.emplace_back(std::move(symbol));
    }
}

void IRParser::_parse_instructions() noexcept {
    LUISA_INFO("parse instructions");
    // INSTRUCTION
    // opcode1 arg1 arg2 ... EOL
    // opcode2 arg1 arg2 ... EOL
    // ...
    // EOF
    while (!_eof()) {
        _skip_empty_lines();
        _skip_whitespaces();
        // parse the instruction
        auto opcode = _parse_identifier();
        LUISA_INFO("parse opcode: {}", opcode);
        // end of instructions, break
        if (opcode == "end"sv) { break; }
        // parse the arguments
        eastl::vector<const Symbol *> args;
        while (!_eol()) {
            _skip_whitespaces();
            auto arg = _parse_identifier();
            LUISA_INFO("parse arg: {}", arg);
            // find the symbol in the symbol map
            auto iter = m_symbol_map.find(arg);
            LUISA_ASSERT(iter != m_symbol_map.end(),
                         "Unknown symbol '{}' at {}.", arg, _location());
            args.emplace_back(iter->second);
            _skip_whitespaces();
        }
        _match_eol();
        // add the instruction to the instruction list
        m_instructions.emplace_back(eastl::make_unique<Instruction>(opcode, std::move(args)));
    }
}

eastl::unique_ptr<Symbol> IRParser::_parse_symbol() noexcept {
    auto backup = _backup();
    _skip_whitespaces();
    auto op = _parse_identifier();
    LUISA_INFO("parse symbol: {}", op);

    auto tag = [&op]() noexcept -> luisa::optional<Symbol::Tag> {
        if (op == "param") { return Symbol::Tag::SYM_PARAM; }
        if (op == "local") { return Symbol::Tag::SYM_LOCAL; }
        if (op == "temp") { return Symbol::Tag::SYM_TEMP; }
        if (op == "const") { return Symbol::Tag::SYM_CONST; }
        if (op == "builtin") { return Symbol::Tag::SYM_BUILTIN; }
        return luisa::nullopt;
    }();
    if (!tag) {
        _restore(backup);
        return nullptr;
    }
    _skip_whitespaces();
    auto type = _parse_type();
    _skip_whitespaces();
    auto array_len = 0;// the length of initial value array
    auto identifier = _parse_identifier();
    LUISA_INFO("parse identifier: {}", identifier);
    _skip_whitespaces();
    auto initial_values = _parse_initial_values();
    // hint
    _match_eol();

    // find its parent
    Symbol *parent = nullptr;
    // find the last "." in the identifier
    if (auto dot = identifier.find_last_of('.');
        dot != luisa::string ::npos) {
        auto parent_ident = luisa::string_view{identifier}.substr(0u, dot);
        // find parent symbol in symbol map
        auto iter = m_symbol_map.find(parent_ident);
        LUISA_ASSERT(iter != m_symbol_map.end(),
                     "Unknown parent symbol '{}' of '{}' at {}.",
                     parent_ident, identifier, _location());
        parent = iter->second;
    }
    auto symbol = eastl::make_unique<Symbol>(
        *tag, type, array_len, parent, std::move(identifier),
        std::move(initial_values));
    return symbol;
}

IRType *IRParser::_parse_type() noexcept {
    // INT/FLOAT/BUFFER/VECTOR/MATRIX
    auto ident = _parse_identifier();
    LUISA_INFO("parse type: {}", ident);
    eastl::unique_ptr<IRType> type;
    // if find existing type
    if (auto iter = m_type_map.find(ident);
        iter != m_type_map.end()) {
        return iter->second;
    }
    // parse the type
    if (ident == "i32"sv) {
        type = eastl::make_unique<IRType>(IRType::Primitive::I32);
    } else if (ident == "f32"sv) {
        type = eastl::make_unique<IRType>(IRType::Primitive::F32);
    } else if (ident == "u32"sv) {
        type = eastl::make_unique<IRType>(IRType::Primitive::U32);
    } else if (ident == "buffer"sv) {
        // type = eastl::make_unique<IRType>(IRType::Primitive::BUFFER);
        _skip_whitespaces();
        auto prim_type = _parse_type();
        if (prim_type->primitive() == IRType::Primitive::F32) {
            type = eastl::make_unique<IRType>(IRType::Primitive::BUFFER_F32);
        } else if (prim_type->primitive() == IRType::Primitive::I32) {
            type = eastl::make_unique<IRType>(IRType::Primitive::BUFFER_I32);
        } else {
            LUISA_ERROR_WITH_LOCATION("Invalid buffer type at {}.", _location());
        }
    } else if (ident == "vector"sv) {
        type = eastl::make_unique<IRType>(IRType::Primitive::VECTOR);
    } else if (ident == "matrix"sv) {
        type = eastl::make_unique<IRType>(IRType::Primitive::MATRIX);
    } else {
        LUISA_ERROR_WITH_LOCATION("Unknown type '{}' at {}.", ident, _location());
    }

    auto ptr = type.get();
    m_type_map.emplace(type->identifier(), ptr);
    m_types.emplace_back(std::move(type));
    return ptr;
}

eastl::vector<IRLiteral> IRParser::_parse_initial_values() noexcept {
    eastl::vector<IRLiteral> initial_values;
    // initial values list are always listed at the end of line
    while (!_eol()) {
        if (_is_number()) {
            initial_values.emplace_back(_parse_number());
        } else {
            LUISA_ERROR_WITH_LOCATION("Unexpected character '{}' at {}.", _peek(), _location());
        }
        _skip_whitespaces();
    }
    return initial_values;
}

luisa::string IRParser::_parse_identifier() noexcept {
    auto head = _read();
    // LUISA_INFO("parse identifier head: {}", head);
    LUISA_ASSERT(detail::is_identifier_head(head), "Invalid identifier head '{}' : {} at {}.", "Expected [a-zA-Z_].", head, _location());
    luisa::string identifier{head};
    while (!_eol()) {
        auto c = _peek();
        if (!detail::is_identifier_body(c)) { break; }
        identifier.push_back(_read());
    }
    return identifier;
}

double IRParser::_parse_number() noexcept {
    auto p_begin = m_state.source.data();
    auto head = _read();
    LUISA_ASSERT(detail::is_number_head(head), "Invalid number head '{}' at {}.", "Expected [0-9.].", head, _location());

    while (!_eol()) {
        auto c = _peek();
        if (!detail::is_number_head(c)) { break; }
        static_cast<void>(_read());
    }

    if (*p_begin == '+') { ++p_begin; }
    auto p_end = m_state.source.data();
    auto p_ret = p_end;
    auto x = std::strtod(p_begin, const_cast<char **>(&p_ret));

    LUISA_ASSERT(p_ret == p_end, "Invalid number '{}' at {}. ", "Expected [0-9].", luisa::string_view{p_begin, static_cast<size_t>(p_end - p_begin)}, _location());

    return x;
}

}// namespace rbc::ps

// The Toolkit

namespace rbc::ps {

bool IRParser::_eof() const noexcept { return m_state.source.empty(); }
luisa::string IRParser::_location() const noexcept { return luisa::format("({}:{})", m_state.line + 1u, m_state.column + 1u); }

char IRParser::_peek() const noexcept {
    LUISA_ASSERT(!_eof(), "unexpected EOF at {}.", _location());
    return m_state.source.front();
}

bool IRParser::_eol() const noexcept { return _eof() || _peek() == '\n'; }

bool IRParser::_is_number() const noexcept {
    return !_eol() && detail::is_number_head(_peek());
}

char IRParser::_read() noexcept {
    LUISA_ASSERT(!_eof(), "Unexpected EOF at {}", _location());
    auto c = m_state.source.front();
    m_state.source.remove_prefix(1);
    if (c == '\n') {
        m_state.line += 1;
        m_state.column = 0;
    } else {
        m_state.column++;
    }
    return c;
}

void IRParser::_match(char expected) noexcept {
    auto c = _read();
    if (c != expected) {
        LUISA_ERROR_WITH_LOCATION(
            "Unpected character '{}' at {}. Expected '{}'.",
            c, _location(), expected);
    }
}
void IRParser::_match(luisa::string_view expected) noexcept {
    for (auto c : expected) {
        _match(c);
    }
}
void IRParser::_match_eol() noexcept {
    if (_eof()) { return; }
    _match('\n');
}

}// namespace rbc::ps