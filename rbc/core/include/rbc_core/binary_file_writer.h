#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/memory.h>
namespace rbc {
struct RBC_CORE_API BinaryFileWriter {
    FILE *_file;

public:
    BinaryFileWriter(luisa::string const &name, bool append = false);
    BinaryFileWriter(BinaryFileWriter const &) = delete;
    BinaryFileWriter(BinaryFileWriter &&rhs);
    BinaryFileWriter &operator=(BinaryFileWriter const &) = delete;
    BinaryFileWriter &operator=(BinaryFileWriter &&rhs) {
        this->~BinaryFileWriter();
        new (std::launder(this)) BinaryFileWriter{std::move(rhs)};
        return *this;
    }
    ~BinaryFileWriter();
    void set_pos(size_t pos) const;
    [[nodiscard]] size_t pos() const;
    void write(luisa::span<std::byte const> data);
};
}// namespace rbc