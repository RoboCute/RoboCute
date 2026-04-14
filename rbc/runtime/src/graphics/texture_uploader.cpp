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
    ShaderManager::instance()->async_load(counter, "texture_process/blit_shader.bin", _blit_shader);
}
void TextureUploader::upload(
    CommandList &cmdlist,
    HostBufferManager &temp_buffer,
    void *ptr,
    ImageView<float> img) const {
    if ((pixel_storage_size(img.storage(), uint3(img.size().x, 1, 1)) & 511) == 0) {
        cmdlist << img.copy_from(luisa::span(reinterpret_cast<std::byte*>(ptr), img.size_bytes()));
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
        default:
            LUISA_ERROR("Unsupported pixel storage for upload.");
    }
}
void TextureUploader::blit(
    CommandList &cmdlist,
    ImageView<float> src_img,
    ImageView<float> dst_img,
    float2 src_uv_scale,
    float2 src_uv_offset,
    uint2 dst_pixel_offset,
    uint2 dst_blit_size,
    luisa::optional<float> manually_alpha) {
    if (any(dst_pixel_offset + dst_blit_size > dst_img.size())) [[unlikely]] {
        LUISA_ERROR("Blit offset {} + dest size {} > dest-image size {}", dst_pixel_offset, dst_blit_size, dst_img.size());
    }
    uint invalid_float = -1;
    cmdlist << (*_blit_shader)(
                   src_img,
                   dst_img,
                   src_uv_scale,
                   src_uv_offset,
                   dst_pixel_offset,
                   manually_alpha ? *manually_alpha : reinterpret_cast<float &>(invalid_float))
                   .dispatch(dst_blit_size);
}
TextureUploader::~TextureUploader() {}
}// namespace rbc