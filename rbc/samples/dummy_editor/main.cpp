#include "EditorWindow.hpp"
#include <QApplication>
#include <QScreen>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("RoboCute Node Editor");
    app.setOrganizationName("RoboCute");
    app.setApplicationVersion("1.0");
    
    EditorWindow window;
    
    // Center window on screen
    window.move(QApplication::primaryScreen()->availableGeometry().center() - window.rect().center());
    window.show();
    
    return app.exec();
}

