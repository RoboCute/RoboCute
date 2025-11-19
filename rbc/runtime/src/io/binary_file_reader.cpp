#include <rbc_io//binary_file_reader.h>
#include <fstream>
#include <iterator>

namespace rbc
{
void BinaryFileReader::read(luisa::vector<uint8_t>& data) const
{
    using bfstream = std::basic_ifstream<uint8_t, std::char_traits<uint8_t>>;
    using bstreambuf_iterator = std::istreambuf_iterator<uint8_t>;

    data.clear();
    if (bfstream bfs{ _filename, std::ios::in | std::ios::binary | std::ios::ate }; bfs.is_open())
    {
        std::size_t total_size = bfs.tellg();
        bfs.seekg(0, std::ios::beg);
        data.reserve(total_size);
        data.assign(bstreambuf_iterator{ bfs }, bstreambuf_iterator{});
        bfs.close();
    }
}
} // namespace rbc