#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/core/dynamic_module.h>
#include <oidn_denoiser.h>

#include <luisa/backends/ext/dx_config_ext.h>
#include <luisa/backends/ext/vk_config_ext.h>
using namespace luisa;
using namespace luisa::compute;
struct DXOidnCheckerExt : public DirectXDeviceConfigExt {
    [[nodiscard]] bool LoadDXC() const noexcept override { return false; }
};
struct VKOidnCheckerExt : public VulkanDeviceConfigExt {
    [[nodiscard]] bool load_dxc() const noexcept override { return false; }
};
int main(int argc, char *argv[]) {
    if (argc != 3) {
        return 1;
    }
    DeviceConfig config{};
    if (luisa::string_view{argv[2]} == "dx") {
        config.extension = luisa::make_unique<DXOidnCheckerExt>();
    } else if (luisa::string_view{argv[2]} == "vk") {
        config.extension = luisa::make_unique<VKOidnCheckerExt>();
    } else if (luisa::string_view{argv[2]} == "cuda") {
        return 0;
    } else {
        return 2;// unsupported backend
    }
    luisa::log_level_error();
    Context ctx{argv[1]};
    Device device = ctx.create_device(argv[2], &config);
    DynamicModule denoiser_module = DynamicModule::load(ctx.runtime_directory(), "oidn_plugin");
    if (!denoiser_module) {
        return 3;// plugin not found.
    }
    vstd::func_ptr_t<rbc::DenoiserExt *(luisa::compute::Device const &)>
        rbc_create_oidn = denoiser_module.function<rbc::DenoiserExt *(luisa::compute::Device const &)>("rbc_create_oidn");
    auto denoise_ext = luisa::unique_ptr<rbc::DenoiserExt>(rbc_create_oidn(device));
    auto denoiser = denoise_ext->create();
    return 0;
}