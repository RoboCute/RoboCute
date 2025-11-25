#include "module_register.h"
#include <rbc_world/async_resource_loader.h>
#include <rbc_world/resource.h>
#include <rbc_world/resource_request.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;

namespace {

void register_resource_bindings(nb::module_& m) {
    // === Resource Enums ===
    
    nb::enum_<rbc::ResourceType>(m, "ResourceType")
        .value("Unknown", rbc::ResourceType::Unknown)
        .value("Mesh", rbc::ResourceType::Mesh)
        .value("Texture", rbc::ResourceType::Texture)
        .value("Material", rbc::ResourceType::Material)
        .value("Shader", rbc::ResourceType::Shader)
        .value("Animation", rbc::ResourceType::Animation)
        .value("Skeleton", rbc::ResourceType::Skeleton)
        .value("PhysicsShape", rbc::ResourceType::PhysicsShape)
        .value("Audio", rbc::ResourceType::Audio)
        .value("Custom", rbc::ResourceType::Custom)
        .export_values();

    nb::enum_<rbc::ResourceState>(m, "ResourceState")
        .value("Unloaded", rbc::ResourceState::Unloaded)
        .value("Pending", rbc::ResourceState::Pending)
        .value("Loading", rbc::ResourceState::Loading)
        .value("Loaded", rbc::ResourceState::Loaded)
        .value("Installed", rbc::ResourceState::Installed)
        .value("Failed", rbc::ResourceState::Failed)
        .value("Unloading", rbc::ResourceState::Unloading)
        .export_values();

    nb::enum_<rbc::LoadPriority>(m, "LoadPriority")
        .value("Critical", rbc::LoadPriority::Critical)
        .value("High", rbc::LoadPriority::High)
        .value("Normal", rbc::LoadPriority::Normal)
        .value("Low", rbc::LoadPriority::Low)
        .value("Background", rbc::LoadPriority::Background)
        .export_values();

    // === AsyncResourceLoader ===
    
    nb::class_<rbc::AsyncResourceLoader>(m, "AsyncResourceLoader")
        .def(nb::init<>())
        .def("initialize", &rbc::AsyncResourceLoader::initialize,
             nb::arg("num_threads") = 4,
             "Initialize the resource loader with specified number of threads")
        .def("shutdown", &rbc::AsyncResourceLoader::shutdown,
             "Shutdown the resource loader and join all threads")
        .def("set_cache_budget", &rbc::AsyncResourceLoader::set_cache_budget,
             nb::arg("bytes"),
             "Set the memory budget for resource cache in bytes")
        .def("get_cache_budget", &rbc::AsyncResourceLoader::get_cache_budget,
             "Get the current memory budget in bytes")
        .def("get_memory_usage", &rbc::AsyncResourceLoader::get_memory_usage,
             "Get the current memory usage in bytes")
        .def("load_resource", &rbc::AsyncResourceLoader::load_resource,
             nb::arg("id"),
             nb::arg("type_value"),
             nb::arg("path"),
             nb::arg("options_json"),
             "Load a resource asynchronously")
        .def("is_loaded", &rbc::AsyncResourceLoader::is_loaded,
             nb::arg("id"),
             "Check if a resource is loaded")
        .def("get_state", &rbc::AsyncResourceLoader::get_state,
             nb::arg("id"),
             "Get the state of a resource")
        .def("get_resource_size", &rbc::AsyncResourceLoader::get_resource_size,
             nb::arg("id"),
             "Get the size of a resource in bytes")
        .def("get_resource_data", &rbc::AsyncResourceLoader::get_resource_data,
             nb::arg("id"),
             nb::rv_policy::reference,
             "Get the raw pointer to resource data")
        .def("unload_resource", &rbc::AsyncResourceLoader::unload_resource,
             nb::arg("id"),
             "Unload a specific resource")
        .def("clear_unused_resources", &rbc::AsyncResourceLoader::clear_unused_resources,
             "Clear unused resources to free memory");
}

// Register using ModuleRegister pattern
ModuleRegister resource_module_register(register_resource_bindings);

} // anonymous namespace

