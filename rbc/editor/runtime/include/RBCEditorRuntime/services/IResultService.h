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

    // === 结果存储 ===
    /**
     * 存储执行结果
     * @param executionId 执行 ID
     * @param nodeId 节点 ID
     * @param outputName 输出名称
     * @param resultData 结果数据 JSON
     * @return 结果 ID
     */
    virtual QString storeResult(const QString &executionId,
                                const QString &nodeId,
                                const QString &outputName,
                                const QJsonObject &resultData) = 0;

    /**
     * 批量存储执行结果
     */
    virtual void storeExecutionResults(
        const QString &executionId,
        const QJsonObject &allResults) = 0;

    // === 结果查询 ===

    /**
     * 获取单个结果
     */
    virtual std::shared_ptr<OutputResult> getResult(const QString &resultId) const = 0;
    /**
     * 按查询条件获取结果列表
     */
    virtual QList<std::shared_ptr<OutputResult>> queryResults(
        const ResultQuery &query) const = 0;

    /**
     * 获取节点的最新输出结果
     */
    virtual std::shared_ptr<OutputResult> getLatestNodeOutput(
        const QString &nodeId,
        const QString &outputName) const = 0;
    /**
     * 获取执行的所有结果
     */
    virtual QList<std::shared_ptr<OutputResult>> getExecutionResults(
        const QString &executionId) const = 0;

    // === 缩略图管理 ===
    /**
     * 获取结果的缩略图
     * @param resultId 结果 ID
     * @param maxSize 最大尺寸
     * @return 缩略图图像
     */
    virtual QImage getThumbnail(
        const QString &resultId,
        const QSize &maxSize = QSize(128, 128)) const = 0;

    /**
     * 异步生成缩略图
     */
    virtual void generateThumbnailAsync(
        const QString &resultId,
        const QSize &maxSize = QSize(128, 128)) = 0;

    // === 结果数据访问 ===
    /**
     * 获取完整的图片数据
     */
    virtual QImage getImageData(const QString &resultId) const = 0;
    /**
     * 获取文本数据
     */
    virtual QString getTextData(const QString &resultId) const = 0;
    /**
     * 获取原始数据
     */
    virtual QByteArray getRawData(const QString &resultId) const = 0;

    // === 结果导出 ===

    /**
     * 保存结果到文件
     */
    virtual bool saveResultToFile(const QString &resultId,
                                  const QString &filePath) const = 0;

    /**
     * 复制结果到剪贴板
     */
    virtual bool copyResultToClipboard(const QString &resultId) const = 0;

    /**
     * 清除指定执行的所有结果
     */
    virtual void clearExecutionResults(const QString &executionId) = 0;

    /**
     * 清除过期结果
     * @param olderThanSeconds 超过此秒数的结果将被清除
     */
    virtual void clearExpiredResults(qint64 olderThanSeconds) = 0;
    /**
     * 清除所有结果
     */
    virtual void clearAllResults() = 0;
    /**
     * 获取缓存大小
     */
    virtual qint64 getCacheSize() const = 0;
    /**
     * 设置最大缓存大小
     */
    virtual void setMaxCacheSize(qint64 bytes) = 0;

signals:
    /**
     * 新结果可用
     */
    void resultAvailable(const QString &resultId,
                         const QString &executionId,
                         const QString &nodeId,
                         const QString &outputName);

    /**
     * 缩略图生成完成
     */
    void thumbnailReady(const QString &resultId);

    /**
     * 结果被清除
     */
    void resultCleared(const QString &resultId);

    /**
     * 执行结果全部到达
     */
    void executionResultsComplete(const QString &executionId);

    /**
     * 缓存大小变化
     */
    void cacheSizeChanged(qint64 currentSize, qint64 maxSize);
};

}// namespace rbc