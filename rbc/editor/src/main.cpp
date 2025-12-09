#include <QApplication>
#include <QFile>
#include "RBCEditor/MainWindow.h"
#include "RBCEditor/EditorEngine.h"

#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>

#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>

#include "RBCEditor/runtime/RenderScene.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

int main(int argc, char **argv) {

#ifdef _WIN32
    // Allocate console window for debug output on Windows
    AllocConsole();
    
    // Redirect stdout and stderr to console
    FILE* pCout;
    FILE* pCerr;
    FILE* pCin;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
    freopen_s(&pCin, "CONIN$", "r", stdin);
    
    // Set console title
    SetConsoleTitleA("RoboCute Editor - Debug Console");
#endif

    int ret = 0;
    log_level_info();

    if (true) {
        QApplication app(argc, argv);
        QFile f(":/main.qss");
        QString styleSheet = "";
        if (f.open(QIODevice::ReadOnly)) {
            styleSheet = f.readAll();
        }
        app.setStyleSheet(styleSheet);
        rbc::EditorEngine::instance().init(argc, argv);
        {
            MainWindow window;
            window.setupUi();
            window.startSceneSync("http://127.0.0.1:5555");
            window.show();
            ret = QApplication::exec();
        }
        rbc::EditorEngine::instance().shutdown();
    }
    return ret;
}
