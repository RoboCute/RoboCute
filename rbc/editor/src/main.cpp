#include <QApplication>
#include <QFile>
#include "RBCEditor/MainWindow.h"
#include "RBCEditor/EditorEngine.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    
    // Load Style
    QFile f(":/main.qss");
    QString styleSheet = "";
    if (f.open(QIODevice::ReadOnly)) {
        styleSheet = f.readAll();
    }
    app.setStyleSheet(styleSheet);

    // Initialize Editor Engine (Runtime)
    rbc::EditorEngine::instance().init(argc, argv);

    int ret = 0;
    {
        // Initialize Main Window (GUI)
        MainWindow window;
        window.setupUi();

        // Start scene synchronization
        window.startSceneSync("http://127.0.0.1:5555");

        window.show();
    
        ret = QApplication::exec();
    } // window is destroyed here, releasing RHI resources safely while Engine is still alive

    // Shutdown Engine
    rbc::EditorEngine::instance().shutdown();
    
    return ret;
}
