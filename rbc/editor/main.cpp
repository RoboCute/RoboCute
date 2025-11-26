#include <QApplication>
#include <QFile>
#include "MainWindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    // Modern Dark Industrial Theme QSS
    QFile f(":/main.qss");
    QString styleSheet = R"(
        /* Global Colors and Fonts */
    )";

    if (f.open(QIODevice::ReadOnly)) {
        styleSheet = f.readAll();
    }

    app.setStyleSheet(styleSheet);

    MainWindow window;
    window.show();

    return app.exec();
}
