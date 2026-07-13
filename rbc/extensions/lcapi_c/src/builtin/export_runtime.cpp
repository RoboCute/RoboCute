#include <fstream>
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include "ref_counter.h"
#include <luisa/backends/ext/cuda/cuda_external_ext.h>
#include <luisa/ast/function.h>
#include <luisa/core/binary_file_stream.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/raster/raster_scene.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/rtx/aabb.h>
#include <luisa/ast/callable_library.h>
#include <luisa/ast/atomic_ref_node.h>
#include <variant>
#include <rbc_graphics/compute_device.h>
#include <rbc_graphics/render_device.h>
#include "module_register.h"
#include "arg_types.h"
#include <rbc_graphics/shader_manager.h>

namespace luisa::compute {
template<typename T>
struct std_make_literal_value {
    static_assert(always_false_v<T>);
};

template<typename... T>
struct std_make_literal_value<std::tuple<T...>> {
    using type = std::variant<T...>;
};

}// namespace luisa::compute
// clang-format off
namespace py = pybind11;
using namespace luisa;
using namespace luisa::compute;
constexpr auto pyref = py::return_value_policy::reference;

class ManagedMeshFormat {
public:
    MeshFormat format;
    luisa::vector<VertexAttribute> attributes;
};
template<typename T>
struct halfN {
    static constexpr bool value = false;
};
template<size_t n>
struct halfN<luisa::Vector<half, n>> {
    static constexpr bool value = true;
    using Type = bool;
    static constexpr size_t dimension = n;
};

void execute_stream() {
    auto ptr= rbc::RenderDevice::instance_ptr();
    if(!ptr) return;
    if(!ptr->lc_main_cmd_list().empty()) {
        ptr->execute_before_cmdlist_commit_task();
        ptr->lc_main_stream() << ptr->lc_main_cmd_list().commit();
    }
    ptr->execute_after_cmdlist_commit_task();
}
void sync_stream() {
    auto ptr= rbc::RenderDevice::instance_ptr();
    if(!ptr) return;
    execute_stream();
    ptr->lc_main_stream().synchronize();
}

PYBIND11_DECLARE_HOLDER_TYPE(T, luisa::shared_ptr<T>)
static std::filesystem::path output_path;
void interop_copy(DeviceInterface &d, uint64_t interop_buffer, uint64_t interop_buffer_offset_bytes, void *cu_stream_ptr, void *cu_buffer, size_t size_bytes, bool interop_to_compute) {
    auto compute_device = rbc::ComputeDevice::instance_ptr();
    if(!compute_device) [[unlikely]] {
        LUISA_ERROR("Compute device not initialized.");
    }
    auto cu_ext = compute_device->get_render_hardware_device()->extension<CUDAExternalExt>();
    auto& stream = rbc::RenderDevice::instance().lc_main_stream();
    execute_stream();
    compute_device->render_to_compute_fence(stream, cu_stream_ptr);
    uint64_t cuda_ptr;
    uint64_t cuda_handle;
    compute_device->cuda_buffer(interop_buffer, &cuda_ptr, &cuda_handle);
    cu_ext->buffer_copy_async(
        interop_to_compute ? cu_buffer : reinterpret_cast<void *>(cuda_ptr),
        interop_to_compute ? reinterpret_cast<void *>(cuda_ptr) : cu_buffer,
        size_bytes,
        cu_stream_ptr);
    rbc::RenderDevice::instance().add_before_cmdlist_commit_task([cu_stream_ptr](){
        auto compute_device = rbc::ComputeDevice::instance_ptr();
        if(compute_device)
            compute_device->compute_to_render_fence(cu_stream_ptr, rbc::RenderDevice::instance().lc_main_stream());
    });
    compute_device->unmap(reinterpret_cast<void *>(cuda_ptr), reinterpret_cast<void *>(cuda_handle));
};
struct VertexData {
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    std::array<float2, 4> uv;
    uint32_t vertex_id;
    uint32_t instance_id;
};
struct AtomicAccessChain {
    using Node = luisa::compute::detail::AtomicRefNode;
    Node const *node{};
};

class UserBinaryIO : public BinaryIO {

private:
    std::filesystem::path _path;

public:
    UserBinaryIO() noexcept {

#ifdef LUISA_PLATFORM_WINDOWS
        auto home = getenv("USERPROFILE");
#else
        auto home = getenv("HOME");
#endif
        if (!home) {
            LUISA_WARNING("Failed to get user home directory: environment variable not found.");
        } else {
            std::error_code ec;
            auto p = std::filesystem::canonical(home, ec);
            if (!ec) {
                _path = p / ".luisa";
            } else {
                LUISA_WARNING("Failed to get user home directory: {}.", ec.message());
            }
        }
        if (_path.empty()) {
            LUISA_WARNING("Failed to get user home directory. Using temporary directory instead.");
            _path = std::filesystem::temp_directory_path() / ".luisa";
        }
        std::error_code ec;
        std::filesystem::create_directories(_path, ec);
        if (ec) {
            LUISA_WARNING("Failed to create application data directory at '{}': {}.",
                          _path.string(), ec.message());
        }
    }

public:
    unique_ptr<BinaryStream> read_shader_bytecode(luisa::string_view name) const noexcept override {
        return luisa::make_unique<BinaryFileStream>(luisa::string{name});
    }
    unique_ptr<BinaryStream> read_shader_cache(luisa::string_view name) const noexcept override {
        if (_path.empty()) { return {}; }
        auto path = _path / "cache" / name;
        return luisa::make_unique<BinaryFileStream>(luisa::string{path.string()});
    }
    unique_ptr<BinaryStream> read_internal_shader(luisa::string_view name) const noexcept override {
        if (_path.empty()) { return {}; }
        auto path = _path / "internal" / name;
        return luisa::make_unique<BinaryFileStream>(luisa::string{path.string()});
    }
    filesystem::path write_shader_bytecode(luisa::string_view name, luisa::span<const std::byte> data) const noexcept override {
        std::filesystem::path path{name};
        if (std::ofstream file{path, std::ios::binary}) {
            file.write(reinterpret_cast<const char *>(data.data()), data.size_bytes());
            return path;
        }
        LUISA_WARNING("Failed to write shader bytecode to '{}'.", name);
        return {};
    }
    void clear_shader_cache() const noexcept override {
        if (_path.empty()) { return; }
        auto cache_path = _path / "cache";
        std::error_code ec;
        std::filesystem::remove_all(cache_path, ec);
        if (ec) {
            LUISA_WARNING("Failed to remove cache directory '{}': {}.",
                          cache_path.string(), ec.message());
        }
    }
    filesystem::path write_shader_cache(luisa::string_view name, luisa::span<const std::byte> data) const noexcept override {
        if (_path.empty()) { return {}; }
        auto cache_path = _path / "cache";
        std::error_code ec;
        std::filesystem::create_directories(cache_path, ec);
        if (ec) {
            LUISA_WARNING("Failed to create application cache directory at '{}': {}.",
                          cache_path.string(), ec.message());
            return {};
        }
        auto path = cache_path / name;
        if (std::ofstream file{path, std::ios::binary}) {
            file.write(reinterpret_cast<const char *>(data.data()), data.size_bytes());
            return path;
        }
        LUISA_WARNING("Failed to write shader cache to '{}'.", path.string());
        return {};
    }
    filesystem::path write_internal_shader(luisa::string_view name, luisa::span<const std::byte> data) const noexcept override {
        if (_path.empty()) { return {}; }
        auto internal_path = _path / "internal";
        std::error_code ec;
        std::filesystem::create_directories(internal_path, ec);
        if (ec) {
            LUISA_WARNING("Failed to create application internal data directory at '{}': {}.",
                          internal_path.string(), ec.message());
            return {};
        }
        auto path = internal_path / name;
        if (std::ofstream file{path, std::ios::binary}) {
            file.write(reinterpret_cast<const char *>(data.data()), data.size_bytes());
            return path;
        }
        LUISA_WARNING("Failed to write internal shader to '{}'.", path.string());
        return {};
    }
};

void export_runtime(py::module &m) {
    // py::class_<ManagedMeshFormat>(m, "MeshFormat")
    //     .def(py::init<>())
    //     .def("add_attribute", [](ManagedMeshFormat &fmt, VertexAttributeType type, VertexElementFormat format) {
    //         fmt.attributes.emplace_back(VertexAttribute{.type = type, .format = format});
    //     })
    //     .def("add_stream", [](ManagedMeshFormat &fmt) {
    //         fmt.format.emplace_vertex_stream(fmt.attributes);
    //         fmt.attributes.clear();
    //     });
    py::class_<ResourceCreationInfo>(m, "ResourceCreationInfo")
        .def(py::init<>())
        .def("handle", [](ResourceCreationInfo &self) { return self.handle; })
        .def("native_handle", [](ResourceCreationInfo &self) { return reinterpret_cast<uint64_t>(self.native_handle); });
    py::class_<BufferCreationInfoInterop>(m, "BufferCreationInfo")
        .def(py::init<>())
        .def("handle", [](BufferCreationInfoInterop &self) { return self.handle; })        
        .def("native_handle", [](BufferCreationInfoInterop &self) { return reinterpret_cast<uint64_t>(self.native_handle); })
        .def("element_size", [](BufferCreationInfoInterop &self) { return self.total_size_bytes / self.element_stride; })
        .def("element_stride", [](BufferCreationInfoInterop &self) { return self.element_stride; })
        .def("total_size_bytes", [](BufferCreationInfoInterop &self) { return self.total_size_bytes; })
        .def("interop", [](BufferCreationInfoInterop &self) { return self.interop; })
        
        .def("set_handle", [](BufferCreationInfoInterop &self, uint64_t handle) { self.handle = handle; })
        .def("set_native_handle", [](BufferCreationInfoInterop &self, uint64_t native_handle) { self.native_handle = reinterpret_cast<void*>(native_handle); })
        .def("set_element_stride", [](BufferCreationInfoInterop &self, size_t element_stride) { self.element_stride = element_stride; })
        .def("set_total_size_bytes", [](BufferCreationInfoInterop &self, size_t total_size_bytes) { self.total_size_bytes = total_size_bytes; })
        .def("set_interop", [](BufferCreationInfoInterop &self, bool interop) { self.interop = interop; })
        ;
    py::class_<TextureCreationInfo>(m, "TextureCreationInfo")
        .def(py::init<>())
        .def("handle", [](TextureCreationInfo &self) { return self.handle; })
        .def("native_handle", [](TextureCreationInfo &self) { return reinterpret_cast<uint64_t>(self.native_handle); })
        .def("format", [](TextureCreationInfo &self) { return self.format; })
        .def("storage", [](TextureCreationInfo &self) { return pixel_format_to_storage(self.format); })
        .def("dimension", [](TextureCreationInfo &self) { return self.dimension; })
        .def("width", [](TextureCreationInfo &self) { return self.width; })
        .def("height", [](TextureCreationInfo &self) { return self.height; })
        .def("depth", [](TextureCreationInfo &self) { return self.depth; })
        .def("mipmap_levels", [](TextureCreationInfo &self) { return self.mipmap_levels; })
        .def("channel", [](TextureCreationInfo& self) {
            switch(pixel_format_to_storage(self.format)) {
                case PixelStorage::BYTE1:
                    return 1;
                case PixelStorage::BYTE2:
                    return 2;
                case PixelStorage::BYTE4:
                case PixelStorage::BYTE4_SRGB:
                    return 4;
                case PixelStorage::SHORT1:
                    return 1;
                case PixelStorage::SHORT2:
                    return 2;
                case PixelStorage::SHORT4:
                    return 4;
                case PixelStorage::INT1:
                    return 1;
                case PixelStorage::INT2:
                    return 2;
                case PixelStorage::INT4:
                    return 4;
                case PixelStorage::HALF1:
                    return 1;
                case PixelStorage::HALF2:
                    return 2;
                case PixelStorage::HALF4:
                    return 4;
                case PixelStorage::FLOAT1:
                    return 1;
                case PixelStorage::FLOAT2:
                    return 2;
                case PixelStorage::FLOAT4:
                    return 4;
                case PixelStorage::R10G10B10A2:
                    return 4;
                case PixelStorage::R11G11B10:
                    return 3;
                case PixelStorage::BC1:
                case PixelStorage::BC4:
                case PixelStorage::BC2:
                case PixelStorage::BC3:
                case PixelStorage::BC5:
                case PixelStorage::BC6:
                case PixelStorage::BC7:
                case PixelStorage::BC7_SRGB:
                    return 4;
                default: return 0;
            }
        })
        ;
    // py::class_<DeviceInterface::BuiltinBuffer>(m, "BuiltinBuffer")
    //     .def("handle", [](DeviceInterface::BuiltinBuffer &buffer) {
    //         return buffer.handle;
    //     })
    //     .def("size", [](DeviceInterface::BuiltinBuffer &buffer) {
    //         return buffer.size;
    //     });
    using StdLiteralType = typename std_make_literal_value<basic_types>::type;
    m.def("to_bytes", [](const StdLiteralType &value) {
        return std::visit(
            [](auto x) noexcept {
                return py::bytes(reinterpret_cast<const char *>(&x), sizeof(x));
            },
            value);
    });


    py::class_<DeviceInterface, luisa::shared_ptr<DeviceInterface>>(m, "DeviceInterface")
        .def("backend_name", [](DeviceInterface &self) {
            return self.backend_name();
        })
        .def("load_shader", [](DeviceInterface &self, luisa::string_view path, ArgTypes const& vec) -> uint64_t {
            py::gil_scoped_release gil_released;
            luisa::filesystem::path relative_path{path};
            if (relative_path.is_relative()) {
                relative_path = rbc::ShaderManager::instance()->shader_path() / relative_path;
            }
            auto info = self.load_shader(luisa::to_string(relative_path), vec.types);
            auto handle = info.handle;
            if(handle == invalid_resource_handle) [[unlikely]] {
                LUISA_ERROR("Shader {} invalid.", path);
            }
            rbc::lcapi_c::RefCounter::current->AddObject(
                handle,
                {[](DeviceInterface *d, uint64_t h) { d->destroy_shader(h); },
                 self.shared_from_this()});
            return handle;
        })

        /*
        0: legal shader
        1: vertex return != pixel arg0
        2: illegal v2p type
        3: pixel output larger than 8
        4: pixel output illegal
        5: not callable
        6: illegal vertex first arguments
        */
        .def("destroy_shader", [](DeviceInterface &self, uint64_t handle) { rbc::lcapi_c::RefCounter::current->DeRef(handle); })
        .def("create_buffer", [](DeviceInterface &d, const Type *type, size_t size) -> BufferCreationInfoInterop{
                py::gil_scoped_release gil_released;
                BufferCreationInfoInterop info;
                vstd::reset(info, d.create_buffer(type, size, nullptr));
                info.interop = false;
                rbc::lcapi_c::RefCounter::current->AddObject(
                    info.handle,
                    {[](DeviceInterface *d, uint64 handle) {
                        sync_stream();
                        d->destroy_buffer(handle);
                     },
                     d.shared_from_this()});
                return info; }, pyref)
        .def("create_interop_buffer", [](DeviceInterface &d, const Type *type, size_t size)-> BufferCreationInfoInterop {
            py::gil_scoped_release gil_released;
            auto &compute_device =  rbc::ComputeDevice::instance();
            BufferCreationInfoInterop info;
            vstd::reset(info, compute_device.create_interop_buffer(type, size));
            info.interop = true;
            rbc::lcapi_c::RefCounter::current->AddObject(
                info.handle,
                {[](DeviceInterface *d, uint64 handle) {
                    sync_stream();
                    d->destroy_buffer(handle);
                },
                d.shared_from_this()});
            return info; }, pyref)
        .def("import_external_buffer", [](DeviceInterface &d, const Type *type, uint64_t native_address, size_t elem_count) noexcept -> BufferCreationInfoInterop {
            py::gil_scoped_release gil_released;
            BufferCreationInfoInterop info;
            vstd::reset(info, d.create_buffer(type, elem_count, reinterpret_cast<void *>(native_address)));
            info.interop = false;
            rbc::lcapi_c::RefCounter::current->AddObject(info.handle, {[](DeviceInterface *d, uint64 handle) {
                sync_stream();
                d->destroy_buffer(handle); }, d.shared_from_this()});
            return info; })
        .def("interop_buffer_copy_from", [](DeviceInterface &d, uint64_t interop_buffer, uint64_t interop_buffer_offset_bytes, uint64_t cu_stream_ptr, uint64_t cu_buffer, size_t size_bytes) { 
            interop_copy(d, interop_buffer, interop_buffer_offset_bytes, reinterpret_cast<void*>(cu_stream_ptr), reinterpret_cast<void*>(cu_buffer), size_bytes, false); 
        })
        .def("interop_buffer_copy_to", [](DeviceInterface &d, uint64_t interop_buffer, uint64_t interop_buffer_offset_bytes, uint64_t cu_stream_ptr, uint64_t cu_buffer, size_t size_bytes) { 
            interop_copy(d, interop_buffer, interop_buffer_offset_bytes,  reinterpret_cast<void*>(cu_stream_ptr), reinterpret_cast<void*>(cu_buffer), size_bytes, true); 
        })
        .def("destroy_buffer", [](DeviceInterface &d, uint64_t handle) { rbc::lcapi_c::RefCounter::current->DeRef(handle); })
        .def("create_texture", [](DeviceInterface &d, PixelFormat format, uint32_t dimension, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipmap_levels)->TextureCreationInfo {
                py::gil_scoped_release gil_released;
                TextureCreationInfo info;
                vstd::reset(info, d.create_texture(format, dimension, width, height, depth, mipmap_levels, nullptr, false, false));
                info.format = format;
                info.dimension = dimension;
                info.width = width;
                info.height = height;
                info.depth = depth;
                info.mipmap_levels = mipmap_levels;
                rbc::lcapi_c::RefCounter::current->AddObject(info.handle, {[](DeviceInterface *d, uint64 handle) {
                    sync_stream();
                    d->destroy_texture(handle); }, d.shared_from_this()});
                    return info; 
                }, pyref)
        .def("destroy_texture", [](DeviceInterface &d, uint64_t handle) { rbc::lcapi_c::RefCounter::current->DeRef(handle); })
        .def(
            "synchronize", [](DeviceInterface &self) { 
                py::gil_scoped_release gil_released;
                sync_stream(); }, pyref)
        .def(
            "add", [](DeviceInterface &self, Command *cmd) { 
                rbc::RenderDevice::instance().lc_main_cmd_list() << luisa::unique_ptr<Command>(cmd);
             }, pyref)
        .def(
            "add_upload_buffer", [](DeviceInterface &self, py::buffer &&buf) { 
                rbc::RenderDevice::instance().add_after_cmdlist_commit_task([buf = std::move(buf)]()mutable {
                    py::gil_scoped_acquire gil_acquired;
                    buf = {};
                });
             }, pyref)
        .def("execute", [](DeviceInterface &self) { 
            py::gil_scoped_release gil_released;
            execute_stream(); }, pyref);
    m.def("get_default_lc_device", [](){
        auto ptr = rbc::RenderDevice::instance_ptr();
        if(!ptr) [[unlikely]] {
            LUISA_ERROR("RBC Context not initialized.");
        }
        luisa::shared_ptr<DeviceInterface> handle = ptr->lc_device().impl_shared();
        return handle;
    });
}
static ModuleRegister module_register_export_runtime(export_runtime);