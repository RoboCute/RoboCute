#pragma once

namespace rbc::ps {

class IRLiteral {
public:
    enum struct Tag : uint32_t {
        NUMBER,
        STRING
    };

private:
    Tag m_tag;
    uint32_t m_string_length;
    union {
        double n;
        char *s;
    } m_value;

public:
    explicit IRLiteral(double n) noexcept;
    explicit IRLiteral(luisa::string_view s) noexcept;
    ~IRLiteral() noexcept;
    IRLiteral(IRLiteral &&) noexcept;
    // delete copy
    IRLiteral(const IRLiteral &) = delete;
    IRLiteral &operator=(const IRLiteral &) = delete;
    IRLiteral &operator=(IRLiteral &&) noexcept = delete;

    // get
    [[nodiscard]] Tag tag() const noexcept { return m_tag; }
    [[nodiscard]] bool is_number() const noexcept { return m_tag == Tag::NUMBER; }
    [[nodiscard]] bool is_string() const noexcept { return m_tag == Tag::STRING; }
    [[nodiscard]] int as_int() const noexcept;
    [[nodiscard]] unsigned int as_uint() const noexcept;
    [[nodiscard]] float as_float() const noexcept;
    [[nodiscard]] luisa::string_view as_string() const noexcept;

    // dump
    [[nodiscard]] luisa::string dump() const noexcept;
};

}// namespace rbc::ps