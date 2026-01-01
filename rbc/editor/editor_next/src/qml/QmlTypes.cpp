#include "RBCEditorRuntime/qml/QmlTypes.h"
#include "RBCEditorRuntime/qml/ConnectionService.h"
#include "RBCEditorRuntime/qml/EditorService.h"
#include <QtQml/qqml.h>

namespace rbc {

void registerQmlTypes() {
    // 注册服务类型
    qmlRegisterType<ConnectionService>("RBCEditor", 1, 0, "ConnectionService");
    qmlRegisterType<EditorService>("RBCEditor", 1, 0, "EditorService");
}

} // namespace rbc

