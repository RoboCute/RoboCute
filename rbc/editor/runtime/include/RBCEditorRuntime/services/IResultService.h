#pragma once
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include <QImage>
#include <luisa/vstl/common.h>

namespace rbc {

enum class ResultType : int {
    Invalid = 0,
    Animation = 1,// Animation Result
    Image = 2,
    Video = 3,
    Mesh = 4,
    PointCloud = 5,
    Custom = 1000
};

// Single Output Result
struct OutputResult {
    QString resultId;   // result unique id
    QString executionId;// execution id
    QString nodeId;     // output Node
    QString outputName; // output Port
    ResultType type;
    QJsonObject metadata;
    QString filePath;
    QByteArray data;
    QImage thumbnail;
    bool hasThumbnail;
    qint64 timestamp;
};

struct ResultMetadata {
    QString id;
    QString name;
    ResultType type;
    QString sourceNode;
    QDateTime timestamp;
    QMap<QString, QVariant> properties;
};

struct ResultQuery {
    QString executionId;
    QString nodeId;
    QString outputName;
    ResultType type;
    int limit = -1;
    bool latestOnly = false;
};

class IResultService : public QObject {
    Q_OBJECT
public:
    virtual ~IResultService() = default;

    // === Result Storage ===
    /**
     * Store execution result
     * @param executionId Execution ID
     * @param nodeId Node ID
     * @param outputName Output name
     * @param resultData Result data JSON
     * @return Result ID
     */
    [[nodiscard]] virtual QString storeResult(const QString &executionId,
                                const QString &nodeId,
                                const QString &outputName,
                                const QJsonObject &resultData) = 0;

    /**
     * Batch store execution results
     */
    virtual void storeExecutionResults(
        const QString &executionId,
        const QJsonObject &allResults) = 0;

    // === Result Query ===

    /**
     * Get a single result
     */
    [[nodiscard]] virtual std::shared_ptr<OutputResult> getResult(const QString &resultId) const = 0;
    /**
     * Get result list by query criteria
     */
    [[nodiscard]] virtual QList<std::shared_ptr<OutputResult>> queryResults(
        const ResultQuery &query) const = 0;

    /**
     * Get the latest output result of a node
     */
    [[nodiscard]] virtual std::shared_ptr<OutputResult> getLatestNodeOutput(
        const QString &nodeId,
        const QString &outputName) const = 0;
    /**
     * Get all results of an execution
     */
    [[nodiscard]] virtual QList<std::shared_ptr<OutputResult>> getExecutionResults(
        const QString &executionId) const = 0;

    // === Thumbnail Management ===
    /**
     * Get result thumbnail
     * @param resultId Result ID
     * @param maxSize Maximum size
     * @return Thumbnail image
     */
    [[nodiscard]] virtual QImage getThumbnail(
        const QString &resultId,
        const QSize &maxSize = QSize(128, 128)) const = 0;

    /**
     * Generate thumbnail asynchronously
     */
    virtual void generateThumbnailAsync(
        const QString &resultId,
        const QSize &maxSize = QSize(128, 128)) = 0;

    // === Result Data Access ===
    /**
     * Get full image data
     */
    [[nodiscard]] virtual QImage getImageData(const QString &resultId) const = 0;
    /**
     * Get text data
     */
    [[nodiscard]] virtual QString getTextData(const QString &resultId) const = 0;
    /**
     * Get raw data
     */
    [[nodiscard]] virtual QByteArray getRawData(const QString &resultId) const = 0;

    // === Result Export ===

    /**
     * Save result to file
     */
    [[nodiscard]] virtual bool saveResultToFile(const QString &resultId,
                                  const QString &filePath) const = 0;

    /**
     * Copy result to clipboard
     */
    [[nodiscard]] virtual bool copyResultToClipboard(const QString &resultId) const = 0;

    /**
     * Clear all results for a specified execution
     */
    virtual void clearExecutionResults(const QString &executionId) = 0;

    /**
     * Clear expired results
     * @param olderThanSeconds Results older than this many seconds will be cleared
     */
    virtual void clearExpiredResults(qint64 olderThanSeconds) = 0;
    /**
     * Clear all results
     */
    virtual void clearAllResults() = 0;
    /**
     * Get cache size
     */
    [[nodiscard]] virtual qint64 getCacheSize() const = 0;
    /**
     * Set maximum cache size
     */
    virtual void setMaxCacheSize(qint64 bytes) = 0;

signals:
    /**
     * New result available
     */
    void resultAvailable(const QString &resultId,
                         const QString &executionId,
                         const QString &nodeId,
                         const QString &outputName);

    /**
     * Thumbnail generation completed
     */
    void thumbnailReady(const QString &resultId);

    /**
     * Result cleared
     */
    void resultCleared(const QString &resultId);

    /**
     * All execution results arrived
     */
    void executionResultsComplete(const QString &executionId);

    /**
     * Cache size changed
     */
    void cacheSizeChanged(qint64 currentSize, qint64 maxSize);
};

}// namespace rbc