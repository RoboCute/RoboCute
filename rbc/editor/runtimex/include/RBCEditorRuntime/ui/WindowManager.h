#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QMainWindow>
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(PluginManager *plugin_mng, QObject *parent = nullptr);

    // == MainWindow Management ==
    QMainWindow *main_window() const { return main_window_; }
    void setup_main_window();

    // == Create Dockable Window View through COntribution (QML)
    QDockWidget *createDockableView(const ViewContribution &contribution, QObject *viewModel);

    // == Create Standalone Window (for Preview)
    QWidget *createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title);


private:
    QMainWindow *main_window_;
};

}// namespace rbc