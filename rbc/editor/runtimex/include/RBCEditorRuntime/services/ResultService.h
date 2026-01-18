#pragma once
#include "RBCEditorRuntime/services/IResultService.h"

namespace rbc {
class ThunbnailWorker : public QObject {
    Q_OBJECT
public:
    void generateThumbnail(const QString &resultId, const QByteArray &imageData, const QSize &maxSize);
signals:
    void thunbmailGenerated(const QString &resultId, const QImage &thumbnail);
};

class ResultService : public IResultService {
    Q_OBJECT
public:
    explicit ResultService(QObject *parent = nullptr);
    ~ResultService();

    // === IResultService Interface Impl ===
    QString storeResult(
        const QString &executionId,
        const QString &nodeId,
        const QString &outputName,
        const QJsonObject &resultData) override;
};

}// namespace rbc