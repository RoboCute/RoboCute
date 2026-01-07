#include "RBCEditorRuntime/services/ResultSerevice.h"

namespace rbc {

QList<IResult *> ResultService::getAllResults() const {
    return {};
}

IResult *ResultService::getResult(const QString &id) const {
    return nullptr;
}

QList<IResult *> ResultService::getResultsByType(ResultType type) const {
    return {};
}

void ResultService::addResult(luisa::unique_ptr<IResult> result) {}
void ResultService::removeResult(const QString &id) {}
void ResultService::clearResults() {}

IPreviewScene *ResultService::createPreviewScene(const QString &resultId) {
    return nullptr;
}

}// namespace rbc