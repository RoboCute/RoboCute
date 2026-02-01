#pragma once
#include <rbc_config.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/core/fiber.h>
#include "host_buffer_manager.h"
#include <rbc_io/io_command_list.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct ShaderManager;
struct DisposeQueue;
struct RBC_RUNTIME_API TextureUploader {
    Shader2D<Buffer<uint>, Image<float>> const *_copy_byte_tex;
    Shader2D<Buffer<half>, Image<float>> const *_copy_half_tex;
    Shader2D<Buffer<float>, Image<float>> const *_copy_float_tex;
    using ImageTensorCopy = Shader2D<
        Buffer<uint>,//&buffer,
        Image<float>,//&img,
        uint2,       //pixel_offset,
        uint,        //channel_map,
        uint,        //channel_idx_scale,
        uint         //channel_idx_offset
        >;
    ImageTensorCopy const *_buffer_to_image;
    ImageTensorCopy const *_image_to_buffer;

public:
    TextureUploader();
    void load_shader(luisa::fiber::counter &counter);
    void upload(
        CommandList &cmdlist,
        HostBufferManager &temp_buffer,
        void *ptr,
        ImageView<float> img) const;
    void upload(
        IOCommandList &io_cmdlist,
        CommandList &cmdlist,
        Device &devive,
        DisposeQueue &disp_queue,
        IOFile::Handle io_file,
        ImageView<float> img,
        uint64_t file_offset = 0) const;
    void upload_with_copy(
        IOCommandList &file_io_cmdlist,
        IOCommandList &mem_io_cmdlist,
        luisa::vector<std::byte> &copy,
        CommandList &cmdlist,
        Device &devive,
        DisposeQueue &disp_queue,
        IOFile::Handle io_file,
        ImageView<float> img,
        uint64_t file_offset = 0) const;
    void upload(
        IOCommandList &io_mem_cmdlist,
        CommandList &cmdlist,
        Device &devive,
        DisposeQueue &disp_queue,
        void const *ptr,
        ImageView<float> img) const;
    void copy(
        CommandList &cmdlist,
        BufferView<uint> src_buffer,
        ImageView<float> dst_img) const;
    enum struct Swizzle : uint8_t {
        X,
        Y,
        Z,
        W
    };
    enum BufferLayout : uint8_t {
        ArrayOfStructure,
        StructureOfArray
    };
    void copy_image_to_buffer(
        CommandList &cmdlist,
        BufferView<uint> buffer,
        ImageView<float> img,
        uint2 pixel_offset,
        uint2 pixel_size,
        luisa::span<Swizzle const> swizzles,
        BufferLayout buffer_layout);
    void copy_buffer_to_image(
        CommandList &cmdlist,
        BufferView<uint> buffer,
        ImageView<float> img,
        uint2 pixel_offset,
        uint2 pixel_size,
        luisa::span<Swizzle const> swizzles,
        BufferLayout buffer_layout);

    ~TextureUploader();
private:
    void _call_buffer_image_copy(
        ImageTensorCopy const *shader,
        CommandList &cmdlist,
        BufferView<uint> buffer,
        ImageView<float> img,
        uint2 pixel_offset,
        uint2 pixel_size,
        luisa::span<Swizzle const> swizzles,
        BufferLayout buffer_layout);
};
}// namespace rbc