#include <rbc_core/binary_file_writer.h>
#include <luisa/core/logging.h>

namespace rbc {

#ifdef _WIN32
#define RBC_FSEEK _fseeki64
#define RBC_FTELL _ftelli64
#else
#define RBC_FSEEK fseeko
#define RBC_FTELL ftello
#endif

BinaryFileWriter::BinaryFileWriter(luisa::string const &name, bool append)
    : _file(nullptr) {
#ifdef _WIN32
    auto err = fopen_s(&_file, name.c_str(), append ? "rb+" : "wb");
    if (err != 0) {
        LUISA_ERROR("Failed to open file '{}': error code {}", name, err);
    }
#else
    _file = fopen(name.c_str(), append ? "rb+" : "wb");
    if (!_file) {
        LUISA_ERROR("Failed to open file '{}'", name);
    }
#endif
}

BinaryFileWriter::BinaryFileWriter(BinaryFileWriter &&rhs) noexcept
    : _file(rhs._file) {
    rhs._file = nullptr;
}

BinaryFileWriter::~BinaryFileWriter() {
    if (_file) {
        fclose(_file);
    }
}

void BinaryFileWriter::set_pos(size_t pos) const {
    if (RBC_FSEEK(_file, static_cast<int64_t>(pos), SEEK_SET) != 0) {
        LUISA_ERROR("Failed to seek to position {} in file", pos);
    }
}

size_t BinaryFileWriter::pos() const {
    auto p = RBC_FTELL(_file);
    if (p < 0) {
        LUISA_ERROR("Failed to get file position");
        return 0;
    }
    return static_cast<size_t>(p);
}

void BinaryFileWriter::write(luisa::span<std::byte const> data) const {
    if (data.empty()) {
        return;
    }
    auto written = fwrite(data.data(), data.size_bytes(), 1, _file);
    if (written != 1) {
        LUISA_ERROR("Failed to write {} bytes to file", data.size_bytes());
    }
}

#undef RBC_FSEEK
#undef RBC_FTELL

}// namespace rbc
