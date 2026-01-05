#include "RBCEditorRuntime/ui/WindowManager.h"

namespace rbc {

WindowManager::WindowManager(PluginManager *plugin_mng, QObject *parent) {
}

void WindowManager::setup_main_window() {
    main_window_ = new QMainWindow();
}

QDockWidget *WindowManager::createDockableView(const ViewContribution &contribution, QObject *viewModel) {
    return nullptr;
}

QWidget *WindowManager::createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title) {
    return nullptr;
}

}// namespace rbc