#include "rbc_particle/ir/instruction.h"

namespace rbc::ps {

luisa::string Instruction::dump() const noexcept {
    luisa::string s = "INSTRUCTION: ";
    s += m_opcode;
    s += " \n";
    auto arg_size = m_args.size();
    for (size_t i = 0; i < arg_size; i++) {
        s += "ARG";
        s += std::to_string(i);
        s += ": ";
        s += m_args[i]->dump();
    }
    return s;
}

}// namespace rbc::ps