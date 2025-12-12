#include <QApplication>
#include <QFile>
#include <string>
#include <cstdlib>

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

#include "RBCEditorRuntime/MainWindow.h"
#include "RBCEditorRuntime/engine/EditorEngine.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

struct AppConfig {
    bool enable_console = true;  // 默认打开 console
    bool enable_debug = true;    // 默认开启 debug 日志
    int server_port = 5555;      // 默认端口 5555
};

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -c, --console          Enable console window (default: enabled)\n");
    printf("  --no-console           Disable console window\n");
    printf("  -d, --debug            Enable debug logging (default: enabled)\n");
    printf("  --no-debug             Disable debug logging\n");
    printf("  -p, --port PORT        Set server port (default: 5555)\n");
    printf("  -h, --help             Show this help message\n");
}

AppConfig parse_arguments(int argc, char **argv) {
    AppConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "-c" || arg == "--console") {
            config.enable_console = true;
        } else if (arg == "--no-console") {
            config.enable_console = false;
        } else if (arg == "-d" || arg == "--debug") {
            config.enable_debug = true;
        } else if (arg == "--no-debug") {
            config.enable_debug = false;
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    config.server_port = std::stoi(argv[++i]);
                    if (config.server_port < 1 || config.server_port > 65535) {
                        fprintf(stderr, "Error: Port must be between 1 and 65535\n");
                        exit(1);
                    }
                } catch (...) {
                    fprintf(stderr, "Error: Invalid port number: %s\n", argv[i]);
                    exit(1);
                }
            } else {
                fprintf(stderr, "Error: --port requires a port number\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", arg.c_str());
            fprintf(stderr, "Use --help for usage information\n");
            exit(1);
        }
    }
    
    return config;
}

int main(int argc, char **argv) {
    // Parse command line arguments
    AppConfig config = parse_arguments(argc, argv);

#ifdef _WIN32
    // Allocate console window for debug output on Windows (if enabled)
    if (config.enable_console) {
        AllocConsole();

        // Redirect stdout and stderr to console
        FILE *pCout;
        FILE *pCerr;
        FILE *pCin;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        freopen_s(&pCerr, "CONOUT$", "w", stderr);
        freopen_s(&pCin, "CONIN$", "r", stdin);

        // Set console title
        SetConsoleTitleA("RoboCute Editor - Debug Console");
    }
#endif

    int ret = 0;
    
    // Set log level based on debug flag
    if (config.enable_debug) {
        log_level_info();
    } else {
        log_level_error();
    }

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
        
        // Build server URL with configured port
        std::string server_url = "http://127.0.0.1:" + std::to_string(config.server_port);
        window.startSceneSync(QString::fromStdString(server_url));
        window.show();
        ret = QApplication::exec();
    }
    rbc::EditorEngine::instance().shutdown();
    
    return ret;
}
