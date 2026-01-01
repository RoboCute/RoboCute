#include "rbc_particle/ir/literal.h"

namespace rbc::ps {

IRLiteral::IRLiteral(double n) noexcept
    : m_tag{Tag::NUMBER}, m_string_length{}, m_value{.n = n} {
}

IRLiteral::IRLiteral(luisa::string_view s) noexcept
    : m_tag{Tag::STRING}, m_string_length{static_cast<uint32_t>(s.size())}, m_value{.s = luisa::allocate_with_allocator<char>(s.size())} {
    // copy string
    std::memcpy(m_value.s, s.data(), s.size());
}
IRLiteral::IRLiteral(IRLiteral &&literal) noexcept
    : m_tag{literal.m_tag}, m_string_length{literal.m_string_length}, m_value{literal.m_value} {
    if (literal.is_string()) {
        literal.m_string_length = 0u;
        literal.m_value.s = nullptr;
    }
}
IRLiteral::~IRLiteral() noexcept {
    // special deallocation for string
    if (is_string() && m_value.s != nullptr) {
        luisa::deallocate_with_allocator(m_value.s);
    }
}

int IRLiteral::as_int() const noexcept {
    if (!is_number()) {
        LUISA_WARNING_WITH_LOCATION(
            "Literal is not an integer.");
        return 0;
    }
    auto i = static_cast<int>(m_value.n);
    if (static_cast<double>(i) != m_value.n) {
        LUISA_WARNING_WITH_LOCATION(
            "Literal is a number but cannot "
            "be converted to integer.");
        return 0;
    }
    return i;
}

unsigned int IRLiteral::as_uint() const noexcept {
    return static_cast<unsigned int>(as_int());
}

float IRLiteral::as_float() const noexcept {
    if (!is_number()) {
        LUISA_WARNING_WITH_LOCATION(
            "Literal is not a float.");
        return 0.f;
    }
    return static_cast<float>(m_value.n);
}

luisa::string_view IRLiteral::as_string() const noexcept {
    if (!is_string()) {
        LUISA_WARNING_WITH_LOCATION(
            "Literal is not a string.");
        return {};
    }
    return {m_value.s, static_cast<size_t>(m_string_length)};
}

luisa::string IRLiteral::dump() const noexcept {
    if (is_number()) {
        return luisa::format("{}", m_value.n);
    }
    luisa::string s{"\""};
    for (auto c : as_string()) {
        // escape special characters
        if (c == '\t') {
            s.append("\\t");
        } else if (c == '\n') {
            s.append("\\n");
        } else if (c == '\r') {
            s.append("\\r");
        } else if (c == '\f') {
            s.append("\\f");
        } else if (c == '\b') {
            s.append("\\b");
        } else if (c == '\\') {
            s.append("\\\\");
        } else if (c == '\"') {
            s.append("\\\"");
        } else if (c == '\0') {
            s.append("\\0");
        } else {
            s.push_back(c);
        }
    }
    s.append("\"");
    return s;
}

}// namespace rbc::ps