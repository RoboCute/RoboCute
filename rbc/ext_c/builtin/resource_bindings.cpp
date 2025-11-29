#include "module_register.h"
#include <rbc_world/async_resource_loader.h>
#include <rbc_world/resource.h>
#include <rbc_world/resource_request.h>
#include <pybind11/pybind11.h>
#include "module_register.h"
namespace py = pybind11;

void register_resource_bindings(py::module &m) {
    // === Resource Enums ===

    py::enum_<rbc::ResourceType>(m, "ResourceType")
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

    py::enum_<rbc::ResourceState>(m, "ResourceState")
        .value("Unloaded", rbc::ResourceState::Unloaded)
        .value("Pending", rbc::ResourceState::Pending)
        .value("Loading", rbc::ResourceState::Loading)
        .value("Loaded", rbc::ResourceState::Loaded)
        .value("Installed", rbc::ResourceState::Installed)
        .value("Failed", rbc::ResourceState::Failed)
        .value("Unloading", rbc::ResourceState::Unloading)
        .export_values();

    py::enum_<rbc::LoadPriority>(m, "LoadPriority")
        .value("Critical", rbc::LoadPriority::Critical)
        .value("High", rbc::LoadPriority::High)
        .value("Normal", rbc::LoadPriority::Normal)
        .value("Low", rbc::LoadPriority::Low)
        .value("Background", rbc::LoadPriority::Background)
        .export_values();

    // === AsyncResourceLoader ===

    py::class_<rbc::AsyncResourceLoader>(m, "AsyncResourceLoader")
        .def(py::init<>())
        .def("initialize", &rbc::AsyncResourceLoader::initialize,
             py::arg("num_threads") = 4,
             "Initialize the resource loader with specified number of threads")
        .def("shutdown", &rbc::AsyncResourceLoader::shutdown,
             "Shutdown the resource loader and join all threads")
        .def("set_cache_budget", &rbc::AsyncResourceLoader::set_cache_budget,
             py::arg("bytes"),
             "Set the memory budget for resource cache in bytes")
        .def("get_cache_budget", &rbc::AsyncResourceLoader::get_cache_budget,
             "Get the current memory budget in bytes")
        .def("get_memory_usage", &rbc::AsyncResourceLoader::get_memory_usage,
             "Get the current memory usage in bytes")
        .def("load_resource", &rbc::AsyncResourceLoader::load_resource,
             "Load a resource asynchronously")
        .def("is_loaded", &rbc::AsyncResourceLoader::is_loaded,
             py::arg("id"),
             "Check if a resource is loaded")
        .def("get_state", &rbc::AsyncResourceLoader::get_state,
             py::arg("id"),
             "Get the state of a resource")
        .def("get_resource_size", &rbc::AsyncResourceLoader::get_resource_size,
             py::arg("id"),
             "Get the size of a resource in bytes")
        .def("get_resource_data", &rbc::AsyncResourceLoader::get_resource_data,
             py::arg("id"),
             py::return_value_policy::reference,
             "Get the raw pointer to resource data")
        .def("unload_resource", &rbc::AsyncResourceLoader::unload_resource,
             py::arg("id"),
             "Unload a specific resource")
        .def("clear_unused_resources", &rbc::AsyncResourceLoader::clear_unused_resources,
             "Clear unused resources to free memory");
}

static ModuleRegister _module_register(register_resource_bindings);