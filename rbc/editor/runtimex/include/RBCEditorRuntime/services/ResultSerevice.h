#pragma once

#include "RBCEditorRuntime/services/IResultService.h"
#include <luisa/vstl/common.h>

namespace rbc {

class ResultService : public IResultService {
    Q_OBJECT
public:
    virtual ~ResultService() = default;

    // === IResultService ===
    QList<IResult *> getAllResults() const override;
    IResult *getResult(const QString &id) const override;
    QList<IResult *> getResultsByType(ResultType type) const override;

    void addResult(luisa::unique_ptr<IResult> result) override;
    void removeResult(const QString &id) override;
    void clearResults() override;
    IPreviewScene *createPreviewScene(const QString &resultId) override;
};

}// namespace rbc