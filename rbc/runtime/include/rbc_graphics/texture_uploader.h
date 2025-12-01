#pragma once
#include <rbc_config.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/core/fiber.h>
#include "host_buffer_manager.h"
#include <rbc_io/io_command_list.h>
namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct DisposeQueue;
struct RBC_RUNTIME_API TextureUploader
{
    Shader2D<Buffer<uint>, Image<float>> const* _copy_byte_tex;
    Shader2D<Buffer<half>, Image<float>> const* _copy_half_tex;
    Shader2D<Buffer<float>, Image<float>> const* _copy_float_tex;

public:
    TextureUploader();
    void load_shader(luisa::fiber::counter& counter);
    void upload(
        CommandList& cmdlist,
        HostBufferManager& temp_buffer,
        void* ptr,
        ImageView<float> img
    ) const;
    void upload(
        IOCommandList& io_cmdlist,
        CommandList& cmdlist,
        Device& devive,
        DisposeQueue& disp_queue,
        IOFile::Handle io_file,
        ImageView<float> img,
        uint64_t file_offset = 0
    ) const;
    void upload_with_copy(
        IOCommandList& file_io_cmdlist,
        IOCommandList& mem_io_cmdlist,
        luisa::vector<std::byte>& copy,
        CommandList& cmdlist,
        Device& devive,
        DisposeQueue& disp_queue,
        IOFile::Handle io_file,
        ImageView<float> img,
        uint64_t file_offset = 0
    ) const;
    void upload(
        IOCommandList& io_mem_cmdlist,
        CommandList& cmdlist,
        Device& devive,
        DisposeQueue& disp_queue,
        void const* ptr,
        ImageView<float> img
    ) const;
    void copy(
        CommandList& cmdlist,
        BufferView<uint> src_buffer,
        ImageView<float> dst_img
    ) const;
    ~TextureUploader();
};
} // namespace rbc