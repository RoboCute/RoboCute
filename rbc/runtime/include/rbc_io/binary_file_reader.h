#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/filesystem.h>

namespace rbc
{
class RBC_RUNTIME_API BinaryFileReader
{
public:
    ~BinaryFileReader() = default;
    BinaryFileReader(BinaryFileReader const&) = delete;
    BinaryFileReader& operator=(BinaryFileReader const&) = delete;
    BinaryFileReader(BinaryFileReader&& rhs) noexcept = default;
    BinaryFileReader& operator=(BinaryFileReader&& rhs) noexcept = default;
    BinaryFileReader(luisa::filesystem::path const& name) noexcept
        : _filename(name)
    {
    }
    [[nodiscard]] bool exists() const noexcept
    {
        std::error_code ec;
        bool exist = luisa::filesystem::exists(_filename, ec);
        return exist && !ec;
    }
    [[nodiscard]] operator bool() const noexcept
    {
        return exists();
    }

    const BinaryFileReader& operator>>(luisa::vector<uint8_t>& data) const
    {
        read(data);
        return *this;
    }

    void read(luisa::vector<uint8_t>& data) const;

private:
    luisa::filesystem::path _filename;
};
} // namespace rbc