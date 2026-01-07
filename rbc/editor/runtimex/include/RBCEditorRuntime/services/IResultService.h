#pragma once
#include <QString>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <luisa/vstl/common.h>
#include "RBCEditorRuntime/infra/render/app_base.h"

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

struct ResultMetadata {
    QString id;
    QString name;
    ResultType type;
    QString sourceNode;
    QDateTime timestamp;
    QMap<QString, QVariant> properties;
};

struct AnimationResultData {
    QString name;
    int totalFrames;
    float fps;
    // const AnimationClip *clip;// Raw Animation Data
    QList<int> entityIds;// Relevant Entity IDs
};

class IResult {
public:
    virtual ~IResult() = default;
    virtual ResultMetadata metadata() const = 0;
    virtual ResultType type() const = 0;
    virtual QVariant data() const = 0;
};

class AnimationResult : public IResult {
public:
    explicit AnimationResult(const ResultMetadata &meta, const AnimationResultData &data);
    ResultMetadata metadata() const override;
    ResultType type() const override { return ResultType::Animation; }
    QVariant data() const override;

    const AnimationResultData &animation_data() const { return data_; }
private:
    ResultMetadata metadata_;
    AnimationResultData data_;
};

class IPreviewScene {
public:
    virtual ~IPreviewScene() = default;

    // Scene Control
    virtual void loadResult(const QString &resultId) = 0;
    virtual void clear() = 0;

    // Animation Control
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void setFrame(int frame) = 0;
    virtual int currentFrame() const = 0;
    virtual bool isPlaying() const = 0;

    // Render Output (for UI)
    virtual IRenderer *renderer() = 0;
};

class IResultService : public QObject {
    Q_OBJECT
public:
    virtual ~IResultService() = default;

    // Result Query
    virtual QList<IResult *> getAllResults() const = 0;
    virtual IResult *getResult(const QString &id) const = 0;
    virtual QList<IResult *> getResultsByType(ResultType type) const = 0;

    // Result Management
    virtual void addResult(luisa::unique_ptr<IResult> result) = 0;
    virtual void removeResult(const QString &id) = 0;
    virtual void clearResults() = 0;

    // Result Preview
    virtual IPreviewScene *createPreviewScene(const QString &resultId) = 0;
};

}// namespace rbc