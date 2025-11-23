#pragma once
#include <rbc_graphics/texture/hdri.h>
#include <luisa/core/fiber.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
struct SkyAtmosphere {
private:
#include <hdri_shader/sky_atmosphere_args.inl>
    Device &_device;
    HDRI &_hdri;
    std::atomic_bool _atmosphere_dirty = true;
    std::atomic_bool _img_initialized = false;
    Shader2D<
        Image<float>,// src_img
        Image<float>,// dst_img
        float,       // max_lum
        uint2,       // blur offset
        uint         // blur radius
        > const *_blur_radius;
    Shader2D<
        Image<float>,// src_img
        float3       // max_lum
        > const *_color_sky;
    Shader2D<
        Image<float>,// src_img,
        float3,      // sun_color,
        float3,      // sun_dir,
        float        // sun_angle_radians
        > const *_make_sun;
    // Shader2D<
    // 	Image<float>,//src_img,
    // 	Image<float> //dst_img,
    // 	> const* _calc_lum;
    Image<float> _src_img;
    Image<float> _img;
    // Image<float> _lum_img;
    Image<float> _temp_img;
    Buffer<float> _weight_buffer;
    Buffer<HDRI::AliasEntry> _alias_table;
    Buffer<float> _pdf_table;
    vstd::LockFreeArrayQueue<luisa::vector<float>> _datas;
    luisa::fiber::counter _event;
    uint2 _size;
    uint _sky_id = ~0u;
    uint _sky_alias_id = ~0u;
    uint _sky_lum_id = ~0u;
    uint _sky_pdf_id = ~0u;
    HDRI::AliasTable _table;
    bool sky_id_dirty : 1 = false;

public:
    bool dirty: 1 = false;
    [[nodiscard]] bool img_initialized() const { return _img_initialized.load(); }
    [[nodiscard]] uint sky_id() const { return _img_initialized.load() ? _sky_id : ~0u; }
    [[nodiscard]] uint sky_alias_id() const { return _img_initialized.load() ? _sky_alias_id : ~0u; }
    [[nodiscard]] uint sky_pdf_id() const { return _img_initialized.load() ? _sky_pdf_id : ~0u; }
    [[nodiscard]] uint sky_lum_id() const { return _img_initialized.load() ? _sky_lum_id : ~0u; }
    SkyAtmosphere(Device &device, HDRI &hdri, Image<float> &&src_img);
    ~SkyAtmosphere();
    void mark_dirty() {
        _atmosphere_dirty = true;
    }
    void make_sun(CommandList &cmdlist, float angle_degree, float3 sun_color, float3 sun_dir);
    void copy_img(CommandList &cmdlist);
    // void calc_lum(CommandList& cmdlist);
    void clamp_light(CommandList &cmdlist, float max_lum, uint blur_pixel);
    void colored(CommandList &cmdlist, float3 color);
    bool update(CommandList &cmdlist, Stream& stream, BindlessAllocator &bdls_alloc, bool force_sync);
    void sync();
    void deallocate(BindlessAllocator &bdls_alloc);
};
}// namespace rbc