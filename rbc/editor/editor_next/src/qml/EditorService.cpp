#include "RBCEditorRuntime/qml/EditorService.h"
#include <QDebug>

namespace rbc {

EditorService::EditorService(QObject *parent)
    : QObject(parent) {
}

void EditorService::initialize() {
    if (m_initialized) {
        return;
    }

    qDebug() << "EditorService: Initializing...";
    
    // 这里可以初始化其他服务
    // 例如：EditorEngine、SceneService 等
    
    m_initialized = true;
    emit initialized();
}

void EditorService::shutdown() {
    if (!m_initialized) {
        return;
    }

    qDebug() << "EditorService: Shutting down...";
    
    m_initialized = false;
    emit shutdownCompleted();
}

} // namespace rbc

