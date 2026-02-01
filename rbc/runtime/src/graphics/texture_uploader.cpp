#include <rbc_graphics/texture_uploader.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/render_device.h>
#include <luisa/dsl/sugar.h>
namespace rbc {
TextureUploader::TextureUploader() {}
void TextureUploader::load_shader(luisa::fiber::counter &counter) {
    ShaderManager::instance()->async_load(counter, "texture_process/copy_byte_tex.bin", _copy_byte_tex);
    if (RenderDevice::instance().backend_name() != "vk")
        ShaderManager::instance()->async_load(counter, "texture_process/copy_half_tex.bin", _copy_half_tex, false);
    ShaderManager::instance()->async_load(counter, "texture_process/copy_float_tex.bin", _copy_float_tex);
    ShaderManager::instance()->async_load(counter, "texture_process/buffer_to_image.bin", _buffer_to_image);
    ShaderManager::instance()->async_load(counter, "texture_process/image_to_buffer.bin", _image_to_buffer);
}
void TextureUploader::upload(
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    void *ptr,
    ImageView<float> img) const {
    if ((pixel_storage_size(img.storage(), uint3(img.size().x, 1, 1)) & 511) == 0) {
        cmdlist << img.copy_from(ptr);
    } else {
        auto buffer = temp_buffer.allocate_upload_buffer<uint>(img.size_bytes() / sizeof(uint));
        std::memcpy(buffer.mapped_ptr(), ptr, img.size_bytes());
        copy(cmdlist, buffer.view, img);
    }
}
void TextureUploader::upload_with_copy(
    IOCommandList &file_io_cmdlist,
    IOCommandList &mem_io_cmdlist,
    luisa::vector<std::byte> &copy_buffer,
    CommandList &cmdlist,
    Device &devive,
    DisposeQueue &disp_queue,
    IOFile::Handle io_file,
    ImageView<float> img,
    uint64_t file_offset) const {
    auto pixel_size = pixel_storage_size(img.storage(), uint3(img.size().x, 1, 1));
    copy_buffer.clear();
    if ((pixel_size & 511) == 0) {
        copy_buffer.push_back_uninitialized(pixel_size);
        file_io_cmdlist << IOCommand{
            io_file,
            file_offset,
            copy_buffer};
        mem_io_cmdlist << IOCommand{
            copy_buffer.data(),
            0,
            IOTextureSubView{img}};
    } else {
        auto buffer = devive.create_buffer<uint>(img.size_bytes() / sizeof(uint));
        copy_buffer.push_back_uninitialized(buffer.size_bytes());
        file_io_cmdlist << IOCommand{
            io_file,
            file_offset, copy_buffer};
        mem_io_cmdlist << IOCommand{
            copy_buffer.data(),
            0,
            IOBufferSubView{buffer}};
        copy(cmdlist, buffer, img);
        disp_queue.dispose_after_queue(std::move(buffer));
    }
}
void TextureUploader::upload(
    IOCommandList &io_cmdlist,
    CommandList &cmdlist,
    Device &devive,
    DisposeQueue &disp_queue,
    IOFile::Handle io_file,
    ImageView<float> img,
    uint64_t file_offset) const {
    if ((pixel_storage_size(img.storage(), uint3(img.size().x, 1, 1)) & 511) == 0) {
        io_cmdlist << IOCommand{
            io_file,
            file_offset, IOTextureSubView{img}};
    } else {
        auto buffer = devive.create_buffer<uint>(img.size_bytes() / sizeof(uint));
        io_cmdlist << IOCommand{
            io_file,
            file_offset, IOBufferSubView{buffer}};
        copy(cmdlist, buffer, img);
        disp_queue.dispose_after_queue(std::move(buffer));
    }
}
void TextureUploader::upload(
    IOCommandList &io_mem_cmdlist,
    CommandList &cmdlist,
    Device &devive,
    DisposeQueue &disp_queue,
    void const *ptr,
    ImageView<float> img) const {
    if ((pixel_storage_size(img.storage(), uint3(img.size().x, 1, 1)) & 511) == 0) {
        io_mem_cmdlist << IOCommand{
            ptr, 0,
            IOTextureSubView{img}};
    } else {
        auto buffer = devive.create_buffer<uint>(img.size_bytes() / sizeof(uint));
        io_mem_cmdlist << IOCommand{
            ptr,
            0, IOBufferSubView{buffer}};
        copy(cmdlist, buffer, img);
        disp_queue.dispose_after_queue(std::move(buffer));
    }
}
void TextureUploader::copy(
    CommandList &cmdlist,
    BufferView<uint> src_buffer,
    ImageView<float> dst_img) const {
    LUISA_ASSERT(dst_img.storage() == PixelStorage::BYTE4 || dst_img.storage() == PixelStorage::HALF4 || dst_img.storage() == PixelStorage::FLOAT4, );
    LUISA_ASSERT(src_buffer.size_bytes() == dst_img.size_bytes(), "Size not match.");
    if ((pixel_storage_size(dst_img.storage(), uint3(dst_img.size().x, 1, 1)) & 511) == 0) {
        cmdlist << dst_img.copy_from(src_buffer);
        return;
    }
    switch (dst_img.storage()) {
        case PixelStorage::BYTE4:
            cmdlist << (*_copy_byte_tex)(
                           src_buffer,
                           dst_img)
                           .dispatch(dst_img.size());
            break;
        case PixelStorage::HALF4:
            LUISA_ASSERT(_copy_half_tex, "Half not support at current backend.");
            cmdlist << (*_copy_half_tex)(
                           src_buffer.as<half>(),
                           dst_img)
                           .dispatch(dst_img.size());
            break;

        case PixelStorage::FLOAT4:
            cmdlist << (*_copy_float_tex)(
                           src_buffer.as<float>(),
                           dst_img)
                           .dispatch(dst_img.size());
            break;
    }
}
void TextureUploader::_call_buffer_image_copy(
    ImageTensorCopy const *shader,
    CommandList &cmdlist,
    BufferView<uint> buffer,
    ImageView<float> img,
    uint2 pixel_offset,
    uint2 pixel_size,
    luisa::span<Swizzle const> swizzles,
    BufferLayout buffer_layout) {
    LUISA_ASSERT(swizzles.size() <= 4, "Swizzle {} out or range 4", swizzles.size());
    LUISA_ASSERT(all(pixel_size <= (img.size() - pixel_offset)), "Pixel size {} out of range {}-{}", pixel_size, img.size(), pixel_offset);
    uint swizzle{~0u};
    for (auto idx : vstd::range(swizzles.size())) {
        swizzle |= ((uint)swizzles[idx]) << (uint)(idx * 8);
    }
    cmdlist << (*shader)(
                   buffer,
                   img,
                   pixel_offset,
                   swizzle,
                   (buffer_layout == BufferLayout::ArrayOfStructure) ? (uint)(swizzles.size()) : (uint)(1),
                   (buffer_layout == BufferLayout::StructureOfArray) ? (uint)(pixel_size.x * pixel_size.y) : (uint)(1))
                   .dispatch(pixel_size);
}
void TextureUploader::copy_image_to_buffer(
    CommandList &cmdlist,
    BufferView<uint> buffer,
    ImageView<float> img,
    uint2 pixel_offset,
    uint2 pixel_size,
    luisa::span<Swizzle const> swizzles,
    BufferLayout buffer_layout) {
    _call_buffer_image_copy(_image_to_buffer, cmdlist, buffer, img, pixel_offset, pixel_size, swizzles, buffer_layout);
}
void TextureUploader::copy_buffer_to_image(
    CommandList &cmdlist,
    BufferView<uint> buffer,
    ImageView<float> img,
    uint2 pixel_offset,
    uint2 pixel_size,
    luisa::span<Swizzle const> swizzles,
    BufferLayout buffer_layout) {
    _call_buffer_image_copy(_buffer_to_image, cmdlist, buffer, img, pixel_offset, pixel_size, swizzles, buffer_layout);
}
TextureUploader::~TextureUploader() {}
}// namespace rbc