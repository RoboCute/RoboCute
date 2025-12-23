#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
DeviceSparseImage::DeviceSparseImage() {}
DeviceSparseImage::~DeviceSparseImage() {
    if (_sparse_img == nullptr) return;
    auto inst = AssetsManager::instance();
    if (inst) {
        _tex_stream_mng->unload_sparse_img(_sparse_img->uid(), inst->load_stream_disqueue());
    }
}
void DeviceSparseImage::load(
    TexStreamManager *tex_stream,
    luisa::move_only_function<void()> &&init_callback,
    luisa::filesystem::path const &path,
    uint64_t file_offset,
    Sampler sampler,
    PixelStorage storage,
    uint2 size,
    uint mip_level) {
    if (_gpu_load_frame != 0) [[unlikely]] {
        return;
    }
    _gpu_load_frame = std::numeric_limits<uint64_t>::max();
    _tex_stream_mng = tex_stream;
    auto allowed_level = mip_level;
    while (allowed_level > 1 && any((size >> (allowed_level - 1)) < TexStreamManager::chunk_resolution)) {
        allowed_level--;
    }
    auto sparse_img = AssetsManager::instance()->lc_device().create_sparse_image<float>(storage, size, allowed_level);
    auto bindless_index = SceneManager::instance().bindless_allocator().allocate_tex2d(sparse_img, {});
    _heap_idx = bindless_index;
    AssetsManager::instance()->load_thd_queue.push(
        [this_shared = RC{this}, tex_stream, path, file_offset, sparse_img = std::move(sparse_img), init_callback = std::move(init_callback)](LoadTaskArgs const &args) mutable {
            auto ptr = static_cast<DeviceSparseImage *>(this_shared.get());
            if (ptr->_gpu_load_frame != std::numeric_limits<uint64_t>::max())
                return;
            ptr->_gpu_load_frame = args.load_frame;
            auto result = tex_stream->load_sparse_img(std::move(sparse_img), TexStreamManager::FilePath{luisa::to_string(path), file_offset}, ptr->_heap_idx, *args.disp_queue, args.cmdlist, std::move(init_callback));
            ptr->_sparse_img = result.img_ptr;
            ptr->_tex_idx = result.tex_idx;
        });
}
bool DeviceSparseImage::load_finished() const {
    LUISA_ERROR("load_finished should not be used in sparse resource");
    return false;
}
}// namespace rbc