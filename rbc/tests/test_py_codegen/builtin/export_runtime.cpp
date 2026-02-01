#include <fstream>

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <luisa/backends/ext/cuda_external_ext.h>
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
#include "ref_counter.h"
#include "res_creation_info.h"
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
using luisa::compute::detail::FunctionBuilder;
struct IntEval {
    int32_t value;
    bool exist;
};

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
    ptr->execute_before_cmdlist_commit_task();
    if(!ptr->lc_main_cmd_list().empty()) {
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
        auto& cmdlist = rbc::RenderDevice::instance().lc_main_cmd_list();
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
        .def("interop", [](BufferCreationInfoInterop &self) { return self.interop; });
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
        .def("create_shader", [](DeviceInterface &self, Function kernel) {
                auto handle = self.create_shader({}, kernel).handle;
                RefCounter::current->AddObject(handle, {[](DeviceInterface *d, uint64 handle) { d->destroy_shader(handle); }, self.shared_from_this()});
                return handle; },
             pyref)
        .def("save_shader", [](DeviceInterface &self, Function kernel, luisa::string_view str) {
            luisa::string_view str_view;
            luisa::string dst_path_str;
            if (!output_path.empty()) {
                auto dst_path = output_path / std::filesystem::path{str};
                dst_path_str = to_string(dst_path);
                str_view = dst_path_str;
            } else {
                str_view = str;
            }
            ShaderOption option{
                .enable_fast_math = true,
                .enable_debug_info = false,
                .compile_only = true,
                .name = luisa::string{str_view}};
            auto useless = self.create_shader(option, kernel); })
        /*
        0: legal shader
        1: vertex return != pixel arg0
        2: illegal v2p type
        3: pixel output larger than 8
        4: pixel output illegal
        5: not callable
        6: illegal vertex first arguments
        */
        .def("destroy_shader", [](DeviceInterface &self, uint64_t handle) { RefCounter::current->DeRef(handle); })
        .def("create_buffer", [](DeviceInterface &d, const Type *type, size_t size) -> BufferCreationInfoInterop{
                BufferCreationInfoInterop info;
                vstd::reset(info, d.create_buffer(type, size, nullptr));
                info.interop = false;
                RefCounter::current->AddObject(
                    info.handle,
                    {[](DeviceInterface *d, uint64 handle) {
                         d->destroy_buffer(handle);
                     },
                     d.shared_from_this()});
                return info; }, pyref)
        .def("create_interop_buffer", [](DeviceInterface &d, const Type *type, size_t size)-> BufferCreationInfoInterop {
            auto &compute_device =  rbc::ComputeDevice::instance();
            BufferCreationInfoInterop info;
            vstd::reset(info, compute_device.create_interop_buffer(type, size));
            info.interop = true;
            RefCounter::current->AddObject(
                info.handle,
                {[](DeviceInterface *d, uint64 handle) {
                    sync_stream();
                    d->destroy_buffer(handle);
                },
                d.shared_from_this()});
            return info; }, pyref)
        .def("import_external_buffer", [](DeviceInterface &d, const Type *type, uint64_t native_address, size_t elem_count) noexcept -> BufferCreationInfoInterop {
            BufferCreationInfoInterop info;
            vstd::reset(info, d.create_buffer(type, elem_count, reinterpret_cast<void *>(native_address)));
            info.interop = false;
            RefCounter::current->AddObject(info.handle, {[](DeviceInterface *d, uint64 handle) {
               sync_stream();
                 d->destroy_buffer(handle); }, d.shared_from_this()});
            return info; })
        .def("interop_buffer_copy_from", [](DeviceInterface &d, uint64_t interop_buffer, uint64_t interop_buffer_offset_bytes, uint64_t cu_stream_ptr, uint64_t cu_buffer, size_t size_bytes) { 
            interop_copy(d, interop_buffer, interop_buffer_offset_bytes, reinterpret_cast<void*>(cu_stream_ptr), reinterpret_cast<void*>(cu_buffer), size_bytes, false); 
        })
        .def("interop_buffer_copy_to", [](DeviceInterface &d, uint64_t interop_buffer, uint64_t interop_buffer_offset_bytes, uint64_t cu_stream_ptr, uint64_t cu_buffer, size_t size_bytes) { 
            interop_copy(d, interop_buffer, interop_buffer_offset_bytes,  reinterpret_cast<void*>(cu_stream_ptr), reinterpret_cast<void*>(cu_buffer), size_bytes, true); 
        })
        .def("destroy_buffer", [](DeviceInterface &d, uint64_t handle) { RefCounter::current->DeRef(handle); })
        .def("create_texture", [](DeviceInterface &d, PixelFormat format, uint32_t dimension, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipmap_levels)->TextureCreationInfo {
                TextureCreationInfo info;
                vstd::reset(info, d.create_texture(format, dimension, width, height, depth, mipmap_levels, nullptr, false, false));
                info.format = format;
                info.dimension = dimension;
                info.width = width;
                info.height = height;
                info.depth = depth;
                info.mipmap_levels = mipmap_levels;
                RefCounter::current->AddObject(info.handle, {[](DeviceInterface *d, uint64 handle) {
                    sync_stream();
                    d->destroy_texture(handle); }, d.shared_from_this()});
                    return info; 
                }, pyref)
        .def("destroy_texture", [](DeviceInterface &d, uint64_t handle) { RefCounter::current->DeRef(handle); })
        .def(
            "synchronize", [](DeviceInterface &self) { sync_stream(); }, pyref)
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
        .def("execute", [](DeviceInterface &self) { execute_stream(); }, pyref);
    m.def("get_default_lc_device", [](){
        auto ptr = rbc::RenderDevice::instance_ptr();
        if(!ptr) [[unlikely]] {
            LUISA_ERROR("RBC Context not initialized.");
        }
        luisa::shared_ptr<DeviceInterface> handle = ptr->lc_device().impl_shared();
        return handle;
    });
    m.def("builder", &FunctionBuilder::current, pyref);
    // m.def("begin_analyzer", [](bool enabled) {
    //     analyzer.emplace_back(enabled ? luisa::make_optional<ASTEvaluator>() : luisa::nullopt);
    // });
    // m.def("end_analyzer", []() {
    //     analyzer.pop_back();
    // });
    // m.def("begin_branch", [](bool is_loop) {
    //     if (auto &&a = analyzer.back()) {
    //         a->begin_branch_scope(is_loop);
    //     }
    // });
    // m.def("end_branch", []() {
    //     if (auto &&a = analyzer.back()) {
    //         a->end_branch_scope();
    //     }
    // });
    // m.def("begin_switch", [](SwitchStmt const *stmt) {
    //     if (auto &&a = analyzer.back()) {
    //         a->begin_switch(stmt);
    //     }
    // });
    // m.def("end_switch", []() {
    //     if (auto &&a = analyzer.back()) {
    //         a->end_switch();
    //     }
    // });
    // m.def("analyze_condition", [](Expression const *expr) -> int32_t {
    //     ASTEvaluator::Result result;
    //     if (auto &&a = analyzer.back()) { result = a->try_eval(expr); }
    //     return visit(
    //         [&]<typename T>(T const &t) -> int32_t {
    //             if constexpr (std::is_same_v<T, bool>) {
    //                 if (t) {
    //                     return 0;// true
    //                 } else {
    //                     return 1;// false
    //                 }
    //             } else {
    //                 return 2;// unsure
    //             }
    //         },
    //         result);
    // });
    py::class_<Function>(m, "Function")
        .def("argument_size", [](Function &func) { return func.arguments().size() - func.bound_arguments().size(); });
    py::class_<IntEval>(m, "IntEval")
        .def("value", [](IntEval &self) { return self.value; })
        .def("exist", [](IntEval &self) { return self.exist; });
    py::class_<CallableLibrary>(m, "CallableLibrary")
        .def(py::init<>())
        .def("add_callable", &CallableLibrary::add_callable)
        .def("serialize", [](CallableLibrary &self, luisa::string_view path) {
            auto vec = self.serialize();
            luisa::string path_str{path};
            auto f = fopen(path_str.c_str(), "wb");
            if (f) {
                fwrite(vec.data(), vec.size(), 1, f);
                LUISA_INFO("Save serialized callable with size: {} bytes.", vec.size());
                fclose(f);
            }
        })
        .def("load", [](CallableLibrary &self, luisa::string_view path) {
            BinaryFileStream file_stream{luisa::string{path}};
            std::vector<std::byte> vec;
            if (file_stream.valid()) {
                vec.resize(vec.size() + file_stream.length());
                file_stream.read(vec);
            }
            self.load(vec);
        });
    py::class_<FunctionBuilder, luisa::shared_ptr<FunctionBuilder>>(m, "FunctionBuilder")
        .def("define_kernel", &FunctionBuilder::define_kernel<const std::function<void()> &>)
        .def("define_callable", &FunctionBuilder::define_callable<const std::function<void()> &>)
        .def("define_raster_stage", &FunctionBuilder::define_raster_stage<const std::function<void()> &>)
        .def("set_block_size", [](FunctionBuilder &self, uint32_t sx, uint32_t sy, uint32_t sz) { self.set_block_size(uint3(sx, sy, sz)); })
        .def("dimension", [](FunctionBuilder &self) {
            if (self.block_size().z > 1) {
                return 3;
            } else if (self.block_size().y > 1) {
                return 2;
            }
            return 1;
        })
        // .def("try_eval_int", [](FunctionBuilder &self, Expression const *expr) {
        //     ASTEvaluator::Result eval;
        //     if (auto &&a = analyzer.back()) { eval = a->try_eval(expr); }
        //     return visit(
        //         [&]<typename T>(T const &t) -> IntEval {
        //             if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>) {
        //                 return {
        //                     .value = static_cast<int32_t>(t),
        //                     .exist = true};
        //             } else {
        //                 return {.value = 0, .exist = false};
        //             }
        //         },
        //         eval);
        // })

        .def("thread_id", &FunctionBuilder::thread_id, pyref)
        .def("block_id", &FunctionBuilder::block_id, pyref)
        .def("dispatch_id", &FunctionBuilder::dispatch_id, pyref)
        .def("kernel_id", &FunctionBuilder::kernel_id, pyref)
        .def("warp_lane_count", &FunctionBuilder::warp_lane_count, pyref)
        .def("warp_lane_id", &FunctionBuilder::warp_lane_id, pyref)
        .def("dispatch_size", &FunctionBuilder::dispatch_size, pyref)

        .def("local", &FunctionBuilder::local, pyref)
        .def("shared", &FunctionBuilder::shared, pyref)
        // .def("shared")
        // .def("constant")
        .def("buffer_binding", &FunctionBuilder::buffer_binding, pyref)
        .def("texture_binding", &FunctionBuilder::texture_binding, pyref)
        .def("bindless_array_binding", &FunctionBuilder::bindless_array_binding, pyref)
        .def("accel_binding", &FunctionBuilder::accel_binding, pyref)

        .def("argument", &FunctionBuilder::argument, pyref)
        .def("reference", &FunctionBuilder::reference, pyref)
        .def("buffer", &FunctionBuilder::buffer, pyref)
        .def("texture", &FunctionBuilder::texture, pyref)
        .def("bindless_array", &FunctionBuilder::bindless_array, pyref)
        .def("accel", &FunctionBuilder::accel, pyref)

        .def("literal", [](FunctionBuilder &self, const Type *type, const StdLiteralType &value)

             {
                LiteralExpr::Value::variant_type value_ptr;
                std::visit(
                    [&](auto&& t){
                        value_ptr = t;
                    },
                    value
                );
                 return luisa::visit(
                   [&self, type]<typename T>(T v) {
                       // we do not allow conversion between vector/matrix/bool types
                       if (type->is_vector() || type->is_matrix() ||
                           type == Type::of<bool>() || type == Type::of<T>()) {
                           return self.literal(type, v);
                       }
                       auto print_v = [&]() {
                           if constexpr (std::is_same_v<std::decay_t<T>, half>) {
                               return (float)v;
                           } else if constexpr (halfN<T>::value) {
                               constexpr auto dim = halfN<T>::dimension;
                               if constexpr (dim == 2) {
                                   return float2((float)v.x, (float)v.y);
                               } else if constexpr (dim == 3) {
                                   return float3((float)v.x, (float)v.y, (float)v.z);
                               } else {
                                   return float4((float)v.x, (float)v.y, (float)v.z, (float)v.z);
                               }
                           } else {
                               return v;
                           }
                       };
                       if constexpr (is_scalar_v<T>) {
                           // we are less strict here to allow implicit conversion
                           // between integral or between floating-point types,
                           // since python does not distinguish them
                           auto safe_convert = [v = print_v()]<typename U>(U /* for tagged dispatch */) noexcept {
                               auto u = static_cast<U>(v);
                               LUISA_ASSERT(static_cast<T>(u) == v,
                                            "Cannot convert literal value {} to type {}.",
                                            v, Type::of<U>()->description());
                               return u;
                           };
                           switch (type->tag()) {
                               case Type::Tag::INT16: return self.literal(type, safe_convert(short{}));
                               case Type::Tag::UINT16: return self.literal(type, safe_convert(luisa::ushort{}));
                               case Type::Tag::INT32: return self.literal(type, safe_convert(int{}));
                               case Type::Tag::UINT32: return self.literal(type, safe_convert(luisa::uint{}));
                               case Type::Tag::INT64: return self.literal(type, safe_convert(luisa::slong{}));
                               case Type::Tag::UINT64: return self.literal(type, safe_convert(luisa::ulong{}));
                               case Type::Tag::FLOAT16: return self.literal(type, static_cast<luisa::half>(v));
                               case Type::Tag::FLOAT32: return self.literal(type, static_cast<float>(v));
                               case Type::Tag::FLOAT64: return self.literal(type, static_cast<double>(v));
                               default: break;
                           }
                       }

                       LUISA_ERROR_WITH_LOCATION(
                           "Cannot convert literal value {} to type {}.",
                           print_v(), type->description());
                   },
                   LiteralExpr::Value{value_ptr}); },
             pyref)
        .def("unary", &FunctionBuilder::unary, pyref)
        .def("binary", &FunctionBuilder::binary, pyref)
        .def("member", &FunctionBuilder::member, pyref)
        .def("access", &FunctionBuilder::access, pyref)
        .def("swizzle", &FunctionBuilder::swizzle, pyref)
        .def("cast", &FunctionBuilder::cast, pyref)

        .def("call", [](FunctionBuilder &self, const Type *type, CallOp call_op, const std::vector<const Expression *> &args) { return self.call(type, call_op, std::move(args)); }, pyref)
        .def("call", [](FunctionBuilder &self, const Type *type, Function custom, const std::vector<const Expression *> &args) { return self.call(type, custom, std::move(args)); }, pyref)
        .def("call", [](FunctionBuilder &self, CallOp call_op, const std::vector<const Expression *> &args) { self.call(call_op, std::move(args)); })
        .def("call", [](FunctionBuilder &self, Function custom, const std::vector<const Expression *> &args) { self.call(custom, std::move(args)); })

        .def("break_", &FunctionBuilder::break_)
        .def("continue_", &FunctionBuilder::continue_)
        .def("return_", &FunctionBuilder::return_)
        .def("assign", [](FunctionBuilder &self, Expression const *l, Expression const *r) { self.assign(l, r); }, pyref)

        .def("if_", &FunctionBuilder::if_, pyref)
        .def("switch_", &FunctionBuilder::switch_, pyref)
        .def("ray_query_", &FunctionBuilder::ray_query_, pyref)
        .def("case_", &FunctionBuilder::case_, pyref)
        .def("loop_", &FunctionBuilder::loop_, pyref)
        // .def("switch_")
        // .def("case_")
        .def("default_", &FunctionBuilder::default_, pyref)
        .def("for_", [](FunctionBuilder &self, const Expression *var, const Expression *condition, const Expression *update) {
                auto ptr = self.for_(var, condition, update);
                return ptr; }, pyref)
        .def("autodiff_", &FunctionBuilder::autodiff_, pyref)
        .def("print_", [](FunctionBuilder &self, luisa::string_view format, const std::vector<const Expression *> &args) { self.print_(luisa::string{format}, args); }, pyref)
        // .def("meta") // unused
        .def("function", &FunctionBuilder::function);// returning object

    py::class_<AtomicAccessChain>(m, "AtomicAccessChain")
        .def(py::init<>())
        .def(
            "create", [&](AtomicAccessChain &self, RefExpr const *buffer_expr) {
                LUISA_ASSERT(self.node == nullptr, "Re-create chain not allowed");
                self.node = AtomicAccessChain::Node::create(buffer_expr);
            },
            pyref)
        .def("access", [&](AtomicAccessChain &self, Expression const *expr) { self.node = self.node->access(expr); }, pyref)
        .def("member", [&](AtomicAccessChain &self, size_t member_index) { self.node = self.node->access(member_index); }, pyref)
        .def("operate", [&](AtomicAccessChain &self, CallOp op, const std::vector<const Expression *> &args) -> Expression const * { return self.node->operate(op, luisa::span<Expression const *const>{args}); }, pyref);
}
static ModuleRegister module_register_export_runtime(export_runtime);