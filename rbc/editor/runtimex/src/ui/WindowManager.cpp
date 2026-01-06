#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include <QQuickWidget>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QDebug>
#include <QUrl>

namespace rbc {

WindowManager::WindowManager(EditorPluginManager *plugin_mng, QObject *parent)
    : QObject(parent)
    , main_window_(nullptr)
    , plugin_mng_(plugin_mng) {
}

void WindowManager::setup_main_window() {
    if (!main_window_) {
        main_window_ = new QMainWindow();
        main_window_->setWindowTitle("RoboCute Editor");
    }
}

QDockWidget *WindowManager::createDockableView(const ViewContribution &contribution, QObject *viewModel) {
    if (!main_window_) {
        qWarning() << "WindowManager::createDockableView: main_window_ is null, call setup_main_window() first";
        return nullptr;
    }

    if (!plugin_mng_ || !plugin_mng_->qmlEngine()) {
        qWarning() << "WindowManager::createDockableView: QML engine is not available";
        return nullptr;
    }

    QQmlEngine *engine = plugin_mng_->qmlEngine();

    // Create QQuickWidget
    QQuickWidget *quickWidget = new QQuickWidget(engine, nullptr);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Resolve QML URL (support qrc:/ and file://)
    QUrl qmlUrl;
    if (contribution.qmlSource.startsWith("qrc:/") || contribution.qmlSource.startsWith(":/")) {
        qmlUrl = QUrl(contribution.qmlSource);
    } else if (contribution.qmlSource.startsWith("file://")) {
        qmlUrl = QUrl(contribution.qmlSource);
    } else {
        // Assume it's a relative path from qrc:/qml/
        qmlUrl = QUrl("qrc:/qml/" + contribution.qmlSource);
    }

    // Set root context property for ViewModel
    QQmlContext *context = quickWidget->rootContext();
    context->setContextProperty("viewModel", viewModel);

    // Load QML
    quickWidget->setSource(qmlUrl);

    if (quickWidget->status() == QQuickWidget::Error) {
        qWarning() << "WindowManager::createDockableView: Failed to load QML:" << qmlUrl;
        qWarning() << "Errors:" << quickWidget->errors();
        delete quickWidget;
        return nullptr;
    }

    // Create DockWidget
    QDockWidget *dock = new QDockWidget(contribution.title, main_window_);
    dock->setObjectName(contribution.viewId);
    dock->setWidget(quickWidget);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // Parse dock area
    Qt::DockWidgetArea dockArea = Qt::LeftDockWidgetArea;
    if (contribution.dockArea == "Right") {
        dockArea = Qt::RightDockWidgetArea;
    } else if (contribution.dockArea == "Top") {
        dockArea = Qt::TopDockWidgetArea;
    } else if (contribution.dockArea == "Bottom") {
        dockArea = Qt::BottomDockWidgetArea;
    } else if (contribution.dockArea == "Center") {
        dockArea = Qt::NoDockWidgetArea; // Center is not a dock area
    }

    // Parse preferred size
    if (!contribution.preferredSize.isEmpty()) {
        QStringList sizeParts = contribution.preferredSize.split(",");
        if (sizeParts.size() == 2) {
            bool ok1, ok2;
            int width = sizeParts[0].trimmed().toInt(&ok1);
            int height = sizeParts[1].trimmed().toInt(&ok2);
            if (ok1 && ok2) {
                quickWidget->setMinimumSize(width, height);
            }
        }
    }

    // Add to main window
    if (dockArea != Qt::NoDockWidgetArea) {
        main_window_->addDockWidget(dockArea, dock);
    }

    qDebug() << "WindowManager::createDockableView: Created dock for" << contribution.viewId;
    return dock;
}

QWidget *WindowManager::createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title) {
    if (!plugin_mng_ || !plugin_mng_->qmlEngine()) {
        qWarning() << "WindowManager::createStandaloneView: QML engine is not available";
        return nullptr;
    }

    QQmlEngine *engine = plugin_mng_->qmlEngine();

    // Create QQuickWidget
    QQuickWidget *quickWidget = new QQuickWidget(engine, nullptr);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setWindowTitle(title);

    // Resolve QML URL
    QUrl qmlUrl;
    if (qmlSource.startsWith("qrc:/") || qmlSource.startsWith(":/")) {
        qmlUrl = QUrl(qmlSource);
    } else if (qmlSource.startsWith("file://")) {
        qmlUrl = QUrl(qmlSource);
    } else {
        qmlUrl = QUrl("qrc:/qml/" + qmlSource);
    }

    // Set root context property
    QQmlContext *context = quickWidget->rootContext();
    context->setContextProperty("viewModel", viewModel);

    // Load QML
    quickWidget->setSource(qmlUrl);

    if (quickWidget->status() == QQuickWidget::Error) {
        qWarning() << "WindowManager::createStandaloneView: Failed to load QML:" << qmlUrl;
        qWarning() << "Errors:" << quickWidget->errors();
        delete quickWidget;
        return nullptr;
    }

    return quickWidget;
}

}// namespace rbc