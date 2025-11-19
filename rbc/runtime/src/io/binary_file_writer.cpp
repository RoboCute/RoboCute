#include <rbc_io//binary_file_writer.h>
#ifdef _WIN32
#define RBC_FSEEK _fseeki64
#define RBC_FTELL _ftelli64
#else
#define RBC_FSEEK fseeko
#define RBC_FTELL ftello
#endif
BinaryFileWriter::BinaryFileWriter(luisa::string const& name, bool append) {
	_file = fopen(name.c_str(), append ? "rb+" : "wb");
}
BinaryFileWriter::BinaryFileWriter(BinaryFileWriter&& rhs) {
	_file = rhs._file;
	rhs._file = nullptr;
}
BinaryFileWriter::~BinaryFileWriter() {
	if (_file) {
		fclose(_file);
	}
}
void BinaryFileWriter::set_pos(size_t pos) const {
	RBC_FSEEK(_file, pos, SEEK_CUR);
}

size_t BinaryFileWriter::pos() const {
	return RBC_FTELL(_file);
}
void BinaryFileWriter::write(luisa::span<std::byte const> data) {
	fwrite(data.data(), data.size_bytes(), 1, _file);
}
#undef RBC_FSEEK
#undef RBC_FTELL