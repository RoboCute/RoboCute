#pragma once

namespace rbc::ps {

class Symbol;
class IRModule;
class IRType;
class Instruction;
class IRLiteral;

class IRParser {
    struct State {
        luisa::string_view source;
        uint32_t line;
        uint32_t column;
    };
    luisa::string m_source;// cache the source code

private:
    State m_state;
    // parser stack
    eastl::vector<eastl::unique_ptr<IRType>> m_types;
    eastl::vector<eastl::unique_ptr<Symbol>> m_symbols;
    eastl::vector<eastl::unique_ptr<Instruction>> m_instructions;

    luisa::unordered_map<luisa::string, Symbol *> m_symbol_map;
    luisa::unordered_map<luisa::string, IRType *> m_type_map;

public:
    IRParser();
    ~IRParser();
    // delete copy
    IRParser(const IRParser &) = delete;
    IRParser &operator=(const IRParser &) = delete;
    // default move
    IRParser(IRParser &&);
    IRParser &operator=(IRParser &&) = delete;

private:
    // element-wise parser
    void _skip_empty_lines() noexcept;
    void _skip_whitespaces() noexcept;

    [[nodiscard]] auto _backup() const noexcept { return m_state; }
    void _restore(const State &state) noexcept;

    [[nodiscard]] eastl::unique_ptr<Symbol> _parse_symbol() noexcept;
    [[nodiscard]] IRType *_parse_type() noexcept;
    [[nodiscard]] eastl::vector<IRLiteral> _parse_initial_values() noexcept;
    [[nodiscard]] double _parse_number() noexcept;
    [[nodiscard]] luisa::string _parse_identifier() noexcept;

    // all traverse method
    void _parse_symbols() noexcept;
    void _parse_instructions() noexcept;

    // parse instruction
    // parse argument

public:
    // method interface
    void set_source(luisa::string source) noexcept;
    void load_file(luisa::string_view file_path) noexcept;

    [[nodiscard]] eastl::unique_ptr<IRModule> parse() noexcept;

private:
    // toolkits
    [[nodiscard]] bool _eof() const noexcept;
    [[nodiscard]] bool _eol() const noexcept;
    [[nodiscard]] luisa::string _location() const noexcept;
    [[nodiscard]] char _peek() const noexcept;
    [[nodiscard]] char _read() noexcept;

    [[nodiscard]] bool _is_number() const noexcept;

    void _match(char expected) noexcept;
    void _match(luisa::string_view expected) noexcept;
    void _match_eol() noexcept;
};

}// namespace rbc::ps