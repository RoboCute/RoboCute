#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QMainWindow>
#include <QDockWidget>
#include "RBCEditorRuntime/plugins/PluginContributions.h"

namespace rbc {

class EditorPluginManager;

class RBC_EDITOR_RUNTIME_API WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(EditorPluginManager *plugin_mng, QObject *parent = nullptr);

    // == MainWindow Management ==
    QMainWindow *main_window() const { return main_window_; }
    void setup_main_window();

    // == Create Dockable Window View through Contribution (QML)
    QDockWidget *createDockableView(const ViewContribution &contribution, QObject *viewModel);

    // == Create Standalone Window (for Preview)
    QWidget *createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title);

private:
    QMainWindow *main_window_;
    EditorPluginManager *plugin_mng_;
};

}// namespace rbc