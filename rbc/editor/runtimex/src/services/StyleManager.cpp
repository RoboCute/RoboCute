#include "RBCEditorRuntime/services/StyleManager.h"

namespace rbc {

StyleManager::StyleManager(QObject *parent) {
}

void StyleManager::initialize(int argc, char **argv) {}

QUrl StyleManager::resolveUrl(const QString &relativePath) {
    return relativePath;
}

}// namespace rbc