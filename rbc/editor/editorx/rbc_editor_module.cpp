#include <QWindow>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QStringList>
#include <QtGlobal>
#include <rbc_config.h>

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget w;
    w.setWindowTitle("EditorX");
    w.show();
    int res = app.exec();
    return res;
}