#include "demo_renderer.h"

#include "RBCEditorRuntime/ui/ViewportWidget.h"
#include <rbc_core/runtime_static.h>
#include <rbc_plugin/plugin_manager.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <QCoreApplication>
#include <QWidget>
#include <QWindow>
#include <QtGui/rhi/qrhi.h>

#include <mutex>
#include <sstream>
#include <unordered_map>

namespace py = pybind11;
using namespace pybind11::literals;

namespace rbc::pyside_demo {

// Per-viewport bookkeeping.  The QWidget is owned by its parent widget once
// reparented; the renderer is owned by us.
struct ViewportEntry {
    QWidget *widget = nullptr;
    std::unique_ptr<DemoRenderer> renderer;
};

static std::mutex g_viewports_mutex;
static std::unordered_map<uintptr_t, ViewportEntry> g_viewports;
static bool g_runtime_initialized = false;

static void ensure_runtime_initialized() {
    if (!g_runtime_initialized) {
        // These mirror the first steps of rbc_editor's main_entry.cpp.
        // They are idempotent, so importing this module alongside the
        // existing robocute extension should be safe.
        rbc::RuntimeStaticBase::init_all();
        rbc::PluginManager::init();
        g_runtime_initialized = true;
    }
}

static QRhi::Implementation backend_from_string(const std::string &backend) {
    if (backend == "dx" || backend == "d3d12") {
        return QRhi::D3D12;
    }
    if (backend == "vk" || backend == "vulkan") {
        return QRhi::Vulkan;
    }
    if (backend == "metal") {
        return QRhi::Metal;
    }
    return QRhi::D3D12;
}

static QWidget *find_widget(uintptr_t wid) {
    if (wid == 0) {
        return nullptr;
    }
    return QWidget::find(static_cast<WId>(wid));
}

/**
 * @brief Create an LC viewport widget and return its native window handle.
 *
 * @param program_path  Path used by LuisaCompute to locate runtime resources
 *                      (e.g. the directory containing shader_build_*).
 * @param backend       RHI backend name: "dx", "vk", "metal".
 * @param parent_wid    Native window handle of the PySide parent widget, or 0.
 *
 * @return Native window handle (WId / HWND on Windows) of the created
 *         ViewportWidget.  PySide can wrap this with shiboken.wrapInstance.
 */
uintptr_t create_viewport(const std::string &program_path,
                          const std::string &backend,
                          uintptr_t parent_wid) {
    if (!QCoreApplication::instance()) {
        throw std::runtime_error(
            "No QCoreApplication instance found. "
            "Create PySide6.QtWidgets.QApplication before calling create_viewport().");
    }

    ensure_runtime_initialized();

    auto renderer = std::make_unique<DemoRenderer>();
    renderer->init(program_path.c_str(), backend.c_str());

    QWidget *parent = find_widget(parent_wid);
    auto graphics_api = backend_from_string(backend);
    auto *widget = new ViewportWidget(renderer.get(), graphics_api, parent);

    uintptr_t wid = widget->winId();

    {
        std::lock_guard lock(g_viewports_mutex);
        ViewportEntry entry;
        entry.widget = widget;
        entry.renderer = std::move(renderer);
        g_viewports.emplace(wid, std::move(entry));
    }

    LUISA_INFO(
        "[rbc_editor_py] create_viewport(program_path='{}', backend='{}', parent_wid={}) -> wid={}",
        program_path, backend, parent_wid, wid);
    return wid;
}

static ViewportEntry *find_entry(uintptr_t wid) {
    std::lock_guard lock(g_viewports_mutex);
    auto it = g_viewports.find(wid);
    if (it == g_viewports.end()) {
        return nullptr;
    }
    return &it->second;
}

void destroy_viewport(uintptr_t wid) {
    std::lock_guard lock(g_viewports_mutex);
    auto it = g_viewports.find(wid);
    if (it == g_viewports.end()) {
        LUISA_WARNING("[rbc_editor_py] destroy_viewport({}): viewport not found", wid);
        return;
    }

    // Deleting the widget triggers ViewportWidget::~ViewportWidget, which
    // releases the swap chain while the native window is still valid.
    if (it->second.widget) {
        delete it->second.widget;
    }
    g_viewports.erase(it);
    LUISA_INFO("[rbc_editor_py] destroy_viewport({})", wid);
}

std::string viewport_info(uintptr_t wid) {
    auto *entry = find_entry(wid);
    if (!entry) {
        return "viewport not found";
    }

    const auto *renderer = entry->renderer.get();
    if (!renderer) {
        return "viewport entry has no renderer";
    }

    std::ostringstream oss;
    oss << "Viewport(wid=" << wid
        << ", initialized=" << renderer->initialized()
        << ", backend=dx"// stub renderer hard-codes dx for display
        << ", camera_distance=" << renderer->camera_distance()
        << ", clear_color=(" << renderer->clear_color()[0] << ","
        << renderer->clear_color()[1] << "," << renderer->clear_color()[2]
        << "), last_key=" << static_cast<int>(renderer->last_key()) << ")";
    return oss.str();
}

void set_camera_distance(uintptr_t wid, float distance) {
    auto *entry = find_entry(wid);
    if (entry && entry->renderer) {
        entry->renderer->set_camera_distance(distance);
    }
}

void set_clear_color(uintptr_t wid, float r, float g, float b) {
    auto *entry = find_entry(wid);
    if (entry && entry->renderer) {
        entry->renderer->set_clear_color(r, g, b);
    }
}

void reset_camera(uintptr_t wid) {
    auto *entry = find_entry(wid);
    if (entry && entry->renderer) {
        entry->renderer->reset_camera();
    }
}

bool has_qapplication() {
    return QCoreApplication::instance() != nullptr;
}

void destroy_all_viewports() {
    std::lock_guard lock(g_viewports_mutex);
    // Collect keys first because deleting widgets may erase map entries.
    std::vector<uintptr_t> keys;
    keys.reserve(g_viewports.size());
    for (const auto &pair : g_viewports) {
        keys.push_back(pair.first);
    }
    for (uintptr_t wid : keys) {
        auto it = g_viewports.find(wid);
        if (it != g_viewports.end()) {
            if (it->second.widget) {
                delete it->second.widget;
            }
            g_viewports.erase(it);
        }
    }
}

}// namespace rbc::pyside_demo

PYBIND11_MODULE(rbc_editor_py, m) {
    using namespace rbc::pyside_demo;

    m.doc() = "Single-process PySide6 + LuisaCompute viewport embedding demo extension.";

    m.def("has_qapplication", &has_qapplication,
          "Return True if a Qt QApplication/QCoreApplication already exists.");

    m.def("create_viewport", &create_viewport,
          "Create an LC-backed ViewportWidget and return its native window handle (WId).",
          py::arg("program_path"), py::arg("backend") = "dx", py::arg("parent_wid") = 0);

    m.def("destroy_viewport", &destroy_viewport,
          "Destroy a viewport created by create_viewport().",
          py::arg("wid"));

    m.def("destroy_all_viewports", &destroy_all_viewports,
          "Destroy every viewport tracked by this module.");

    m.def("viewport_info", &viewport_info,
          "Return diagnostic string for a viewport.",
          py::arg("wid"));

    m.def("set_camera_distance", &set_camera_distance,
          "Adjust the demo camera distance (logged only in stub renderer).",
          py::arg("wid"), py::arg("distance"));

    m.def("set_clear_color", &set_clear_color,
          "Set the demo clear colour (logged only in stub renderer).",
          py::arg("wid"), py::arg("r"), py::arg("g"), py::arg("b"));

    m.def("reset_camera", &reset_camera,
          "Reset the demo camera (logged only in stub renderer).",
          py::arg("wid"));

    // Runtime initialisation is done lazily inside create_viewport so that
    // merely importing the module does not require a QApplication.
}
