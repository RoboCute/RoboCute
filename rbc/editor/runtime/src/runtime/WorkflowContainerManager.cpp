#include "RBCEditorRuntime/runtime/WorkflowContainerManager.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"

#include <QMainWindow>
#include <QDebug>

namespace rbc {

WorkflowContainerManager::WorkflowContainerManager(QMainWindow* mainWindow,
                                                   EditorContext* context,
                                                   QObject* parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      context_(context),
      stackedWidget_(nullptr) {
    Q_ASSERT(mainWindow_ != nullptr);
    Q_ASSERT(context_ != nullptr);
}

void WorkflowContainerManager::initialize() {
    // 创建 StackedWidget
    stackedWidget_ = new QStackedWidget(mainWindow_);
    stackedWidget_->setObjectName("WorkflowStackedWidget");

    // 将 StackedWidget 设置为 MainWindow 的 CentralWidget
    mainWindow_->setCentralWidget(stackedWidget_);

    qDebug() << "WorkflowContainerManager: Initialized with StackedWidget";
}

void WorkflowContainerManager::switchContainer(WorkflowType type) {
    if (!stackedWidget_) {
        qWarning() << "WorkflowContainerManager: StackedWidget not initialized";
        return;
    }

    if (!containerIndices_.contains(type)) {
        qWarning() << "WorkflowContainerManager: No container registered for workflow type"
                   << static_cast<int>(type);
        return;
    }

    int index = containerIndices_[type];
    qDebug() << "WorkflowContainerManager: Switching to container index" << index
             << "for workflow" << static_cast<int>(type);

    stackedWidget_->setCurrentIndex(index);
}

void WorkflowContainerManager::registerContainer(WorkflowType type, QWidget* container) {
    if (!stackedWidget_) {
        qWarning() << "WorkflowContainerManager: StackedWidget not initialized";
        return;
    }

    if (!container) {
        qWarning() << "WorkflowContainerManager: Null container provided";
        return;
    }

    // 将容器添加到 StackedWidget
    int index = stackedWidget_->addWidget(container);
    containerIndices_[type] = index;

    qDebug() << "WorkflowContainerManager: Registered container for workflow"
             << static_cast<int>(type) << "at index" << index;
}

}// namespace rbc

