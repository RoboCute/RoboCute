#include <rbc_render/procedural/sky_atmosphere.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/buffer.h>
namespace rbc {
SkyAtmosphere::SkyAtmosphere(Device &device, HDRI &hdri, RC<DeviceImage> src_img)
    : _device(device), _hdri(hdri), _src_img(std::move(src_img)),
      //   _lum_img(device.create_image<float>(PixelStorage::FLOAT1, _src_img.size())),
      _weight_buffer(device.create_buffer<float>(_src_img->get_float_image().size().x * _src_img->get_float_image().size().y)), _size(_src_img->get_float_image().size()), _event() {
    luisa::fiber::counter shader_init_counter;
    ShaderManager::instance()->async_load(shader_init_counter, "hdri/clamp_lum.bin", _blur_radius);
    ShaderManager::instance()->async_load(shader_init_counter, "hdri/color_sky.bin", _color_sky);
    ShaderManager::instance()->async_load(shader_init_counter, "hdri/make_sun.bin", _make_sun);
    shader_init_counter.wait();
    // sm->load("hdri/calc_lum.bin", _calc_lum);
}

void SkyAtmosphere::copy_img(CommandList &cmdlist) {
    if (!_img)
        _img = _device.create_image<float>(PixelStorage::FLOAT4, _src_img->get_float_image().size());
    cmdlist << _img.copy_from(_src_img->get_float_image());
}

void SkyAtmosphere::colored(CommandList &cmdlist, float3 color) {
    if (!_img)
        _img = _device.create_image<float>(PixelStorage::FLOAT4, _src_img->get_float_image().size());
    cmdlist << (*_color_sky)(
                   _img,
                   color)
                   .dispatch(_img.size());
}

void SkyAtmosphere::make_sun(CommandList &cmdlist, float angle_degree, float3 sun_color, float3 sun_dir) {
    if (!_img)
        _img = _device.create_image<float>(PixelStorage::FLOAT4, _src_img->get_float_image().size());
    cmdlist << (*_make_sun)(
                   _img,
                   sun_color,
                   normalize(sun_dir),
                   radians(angle_degree))
                   .dispatch(_img.size());
}

void SkyAtmosphere::clamp_light(CommandList &cmdlist, float max_lum, uint blur_pixel) {
    if (!_temp_img)
        _temp_img = _device.create_image<float>(PixelStorage::FLOAT4, _src_img->get_float_image().size());
    cmdlist << (*_blur_radius)(
                   _src_img->get_float_image(),
                   _temp_img,
                   max_lum,
                   uint2(1, 0),
                   blur_pixel)
                   .dispatch(_img.size())
            << (*_blur_radius)(
                   _temp_img,
                   _img,
                   max_lum,
                   uint2(0, 1),
                   blur_pixel)
                   .dispatch(_img.size());
}

SkyAtmosphere::~SkyAtmosphere() {
}
// void SkyAtmosphere::calc_lum(CommandList& cmdlist) {
// cmdlist << (*_calc_lum)(_img, _lum_img).dispatch(_img.size());
// }

void SkyAtmosphere::deallocate(BindlessAllocator &bdls_alloc) {
    auto collect = [&](uint id) {
        if (id != ~0u) {
            bdls_alloc.deallocate_buffer(id);
        }
    };
    if (_sky_id != ~0u) {
        bdls_alloc.deallocate_tex2d(_sky_id);
    }
    collect(_sky_alias_id);
    collect(_sky_pdf_id);
}

bool SkyAtmosphere::update(CommandList &cmdlist, Stream &stream, BindlessAllocator &bdls_alloc, bool force_sync) {
    if (!_atmosphere_dirty.load())
        return false;
    auto &img_view = [&]() -> Image<float> const & {
        if (!_img)
            return _src_img->get_float_image();
        return _img;
    }();
    _hdri.compute_scalemap(_device, cmdlist, img_view, _size, _weight_buffer, [this](luisa::vector<float> &&data) {
        _datas.push(std::move(data));
    });
    if (force_sync) {
        stream << cmdlist.commit() << synchronize();
    }
    while (auto data = _datas.pop()) {
        _event.add();
        luisa::fiber::schedule([this, data = std::move(*data)]() mutable {
            auto table = _hdri.compute_alias_table(data, _size);
            if (!_alias_table.valid()) {
                _alias_table = _device.create_buffer<HDRI::AliasEntry>(table.table.size());
            }
            if (!_pdf_table.valid()) {
                _pdf_table = _device.create_buffer<float>(table.pdfs.size());
            }
            _table = std::move(table);
            _event.done();
        });
    }
    if (!force_sync) {
        if (!_event.test())
            return false;
    } else {
        _event.wait();
    }
    _atmosphere_dirty = false;
    bool update = false;
    if (!_table.table.empty()) {
        cmdlist << _alias_table.view().copy_from(_table.table.data());
        _table.table.clear();
        if (_sky_alias_id == ~0u) {
            _sky_alias_id = bdls_alloc.allocate_buffer(_alias_table);
        }
        _img_initialized = true;
        update = true;
    }
    if (!_table.pdfs.empty()) {
        cmdlist << _pdf_table.view().copy_from(_table.pdfs.data());
        _table.pdfs.clear();
        if (_sky_pdf_id == ~0u) {
            _sky_pdf_id = bdls_alloc.allocate_buffer(_pdf_table);
        }
        _img_initialized = true;
        update = true;
    }
    // if (_sky_lum_id == ~0u) {
    // 	_sky_lum_id = bdls_alloc.allocate_tex2d(_lum_img, Sampler::point_edge());
    // 	calc_lum(cmdlist);
    // }

    if (_sky_id == ~0u) {
        _sky_id = bdls_alloc.allocate_tex2d(img_view, Sampler::point_edge());
    } else if (sky_id_dirty) {
        bdls_alloc.image_heap().emplace_on_update(_sky_id, img_view, Sampler::point_edge());
    }
    sky_id_dirty = false;
    return update;
}
void SkyAtmosphere::sync() {
    _event.wait();
}
}// namespace rbc