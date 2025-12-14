#include <rbc_graphics/device_assets/device_buffer.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_io/io_command_list.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
DeviceBuffer::DeviceBuffer() {}
DeviceBuffer::~DeviceBuffer() {
    if (!_buffer.valid()) return;
    auto inst = AssetsManager::instance();
    if (inst) {
        inst->dispose_after_render_frame(std::move(_buffer));
    }
}
void DeviceBuffer::async_load_from_file(
    luisa::filesystem::path const &path,
    size_t file_offset,
    uint64_t file_size,
    FileLoadType load_type) {
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push(
        [this_shared = RC{this}, path, file_offset, load_type, desire_file_size = file_size](
            LoadTaskArgs const &args) {
            auto ptr = static_cast<DeviceBuffer *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            auto file = args.io_cmdlist.retrieve(luisa::to_string(path));
            auto file_size = file.length() - file_offset;
            if (file_size < desire_file_size) [[unlikely]] {
                LUISA_ERROR("Buffer file size {} less than required size {}", file_size, desire_file_size);
            }
            LUISA_ASSERT(desire_file_size & (sizeof(uint) - 1) == 0);
            ptr->_buffer = inst->lc_device().create_buffer<uint>(desire_file_size / sizeof(uint));
            switch (load_type) {
                case FileLoadType::All: {
                    if (ptr->_host_data.size() != desire_file_size) {
                        ptr->_host_data.clear();
                        ptr->_host_data.push_back_uninitialized(desire_file_size);
                    }
                    *args.require_disk_io_sync = true;
                    args.io_cmdlist << IOCommand(
                        file,
                        file_offset,
                        luisa::span{ptr->_host_data});
                    args.mem_io_cmdlist << IOCommand{
                        IOCommand::SrcType{ptr->_host_data.data()},
                        0,
                        IOBufferSubView{ptr->_buffer}};
                } break;
                case FileLoadType::DeviceOnly: {
                    args.io_cmdlist << IOCommand(
                        file,
                        file_offset,
                        IOBufferSubView{ptr->_buffer});
                } break;
                default: {
                    if (ptr->_host_data.size() != desire_file_size) {
                        ptr->_host_data.clear();
                        ptr->_host_data.push_back_uninitialized(desire_file_size);
                    }
                    args.io_cmdlist << IOCommand(
                        file,
                        file_offset,
                        luisa::span{ptr->_host_data});
                } break;
            }
        });
}
void DeviceBuffer::async_load_from_memory(luisa::vector<BinaryBlob> &&blobs) {
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push(
        [this_shared = RC{this}, blobs = std::move(blobs)](
            LoadTaskArgs const &args) mutable {
            auto ptr = static_cast<DeviceBuffer *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            size_t blob_size = 0;
            for (auto &i : blobs) {
                blob_size += i.size();
            }
            ptr->_buffer = inst->lc_device().create_buffer<uint>(blob_size / sizeof(uint));
            blob_size = 0;
            for (auto &i : blobs) {
                args.mem_io_cmdlist << IOCommand{
                    i.data(),
                    0,
                    ptr->_buffer.view(blob_size / sizeof(uint), i.size() / sizeof(uint))};
                blob_size += i.size();
            }
            args.disp_queue->dispose_after_queue(std::move(blobs));
        });
}
void DeviceBuffer::async_load_from_memory(BinaryBlob &&blob) {
    auto inst = AssetsManager::instance();
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    inst->load_thd_queue.push(
        [this_shared = RC{this}, blob = std::move(blob)](
            LoadTaskArgs const &args) mutable {
            auto ptr = static_cast<DeviceBuffer *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max()) return;
            ptr->_gpu_load_frame = args.load_frame;
            auto inst = AssetsManager::instance();
            LUISA_ASSERT((blob.size() & (sizeof(uint) - 1)) == 0);
            ptr->_buffer = inst->lc_device().create_buffer<uint>(blob.size() / sizeof(uint));
            args.mem_io_cmdlist << IOCommand{
                blob.data(),
                0,
                IOBufferSubView{ptr->_buffer}};
            args.disp_queue->dispose_after_queue(std::move(blob));
        });
}
}// namespace rbc