#pragma once

namespace rbc::ps {

class Symbol;
class Instruction {
private:
    luisa::string m_opcode;
    luisa::vector<const Symbol *> m_args;

public:
    Instruction(luisa::string opcode, luisa::vector<const Symbol *> args)
        : m_opcode(opcode), m_args(args) {
    }
    ~Instruction() noexcept = default;
    // default move
    Instruction(Instruction &&) noexcept = default;
    Instruction &operator=(Instruction &&) noexcept = default;
    // disable copy
    Instruction(const Instruction &) = delete;
    Instruction &operator=(const Instruction &) noexcept = delete;
    [[nodiscard]] auto opcode() const noexcept { return luisa::string_view{m_opcode}; }
    [[nodiscard]] auto args() const noexcept { return luisa::span{m_args}; }

    // for debugging
    [[nodiscard]] luisa::string dump() const noexcept;
};

}// namespace rbc::ps