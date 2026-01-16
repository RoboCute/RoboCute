# Graph Service & Result Service

## 概述

GraphService 和 ResultService 是 RoboCute Editor 的核心服务，负责管理节点图的执行和结果展示。

- **GraphService**：管理节点图的定义加载、编辑状态、执行请求以及与 Remote Server 的通信
- **ResultService**：管理执行结果的存储、查询、预览和生命周期

这两个服务与编辑器内专用的 **Preview Node** 协同工作，提供完整的节点图执行和结果预览体验。

## 核心功能

### GraphService

1. **节点定义管理**：从 Remote Server 加载节点类型定义，维护可用节点注册表
2. **图编辑状态管理**：跟踪当前编辑的节点图，管理撤销/重做历史
3. **执行请求管理**：向 Remote Server 提交执行请求，跟踪执行状态
4. **多图支持**：支持同时打开和管理多个节点图
5. **连接状态监控**：维护与 Remote Server 的连接状态

### ResultService

1. **结果存储**：接收并存储执行结果（图片、文本、场景等）
2. **结果查询**：按执行 ID、节点 ID、结果类型查询结果
3. **结果预览**：生成缩略图，支持结果预览
4. **结果缓存**：管理结果的内存和磁盘缓存
5. **生命周期管理**：自动清理过期结果

### Preview Node

1. **编辑器专用节点**：不在 Python 节点图中定义，仅存在于编辑器
2. **结果选择**：通过连接选择要预览的节点输出
3. **缩略图显示**：在节点上直接展示结果缩略图
4. **详情预览**：双击打开结果预览窗口
5. **多类型支持**：支持图片、文本预览，未来扩展场景和动画序列

---

## 架构设计

### 1. 系统架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        RoboCute Editor                              │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌──────────────┐    ┌───────────────────────┐   │
│  │NodeEditor   │◄──►│GraphService  │◄──►│  Remote Server (Py)   │   │
│  │  Plugin     │    │              │    │                       │   │
│  └──────┬──────┘    └──────┬───────┘    └───────────────────────┘   │
│         │                  │                      ▲                 │
│         │                  │                      │                 │
│  ┌──────▼──────┐    ┌──────▼───────┐              │                 │
│  │PreviewNode  │◄──►│ResultService │◄─────────────┘                 │
│  │  (Editor)   │    │              │    (Execution Results)         │
│  └──────┬──────┘    └──────────────┘                                │
│         │                                                           │
│  ┌──────▼───────────────────────────────────────────────────────┐   │
│  │                   Preview Window                              │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │   │
│  │  │ ImageViewer │  │ TextViewer  │  │ SceneViewer │ (Future)  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘           │   │
│  └───────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 2. 数据流

```
1. 用户构建节点图
   ↓
2. GraphService.executeGraph() 
   ↓
3. Remote Server 执行节点图
   ↓
4. 执行结果返回
   ↓
5. ResultService 存储结果
   ↓
6. PreviewNode 订阅结果更新
   ↓
7. PreviewNode 显示缩略图
   ↓
8. 用户双击 → 打开预览窗口
```

---

## 数据结构

### 3.1 节点定义 (NodeDefinition)

```json
{
  "node_type": "text2image.StableDiffusion",
  "display_name": "Stable Diffusion",
  "category": "AI/Image Generation",
  "description": "Generate images from text prompts using Stable Diffusion",
  "inputs": [
    {
      "name": "prompt",
      "type": "string",
      "display_name": "Prompt",
      "required": true,
      "default": "",
      "widget": "textarea"
    },
    {
      "name": "negative_prompt",
      "type": "string",
      "display_name": "Negative Prompt",
      "required": false,
      "default": ""
    },
    {
      "name": "width",
      "type": "int",
      "display_name": "Width",
      "required": true,
      "default": 512,
      "min": 64,
      "max": 2048
    },
    {
      "name": "height",
      "type": "int",
      "display_name": "Height",
      "required": true,
      "default": 512,
      "min": 64,
      "max": 2048
    }
  ],
  "outputs": [
    {
      "name": "image",
      "type": "image",
      "display_name": "Generated Image",
      "previewable": true
    }
  ]
}
```

### 3.2 执行请求 (ExecutionRequest)

```json
{
  "execution_id": "exec_20260116_143025_abc123",
  "graph_id": "graph_main",
  "nodes": [
    {
      "node_id": "node_1",
      "node_type": "text2image.StableDiffusion",
      "inputs": {
        "prompt": "a cute robot in a garden",
        "negative_prompt": "",
        "width": 512,
        "height": 512
      }
    },
    {
      "node_id": "node_2",
      "node_type": "image.Upscale",
      "inputs": {
        "scale": 2
      }
    }
  ],
  "connections": [
    {
      "from_node": "node_1",
      "from_output": "image",
      "to_node": "node_2",
      "to_input": "image"
    }
  ],
  "preview_nodes": [
    {
      "preview_node_id": "preview_1",
      "source_node_id": "node_1",
      "source_output": "image"
    },
    {
      "preview_node_id": "preview_2",
      "source_node_id": "node_2",
      "source_output": "image"
    }
  ]
}
```

### 3.3 执行结果 (ExecutionResult)

```json
{
  "execution_id": "exec_20260116_143025_abc123",
  "status": "completed",
  "started_at": "2026-01-16T14:30:25.000Z",
  "completed_at": "2026-01-16T14:30:45.000Z",
  "node_results": {
    "node_1": {
      "status": "completed",
      "outputs": {
        "image": {
          "type": "image",
          "format": "png",
          "width": 512,
          "height": 512,
          "data_url": "data:image/png;base64,...",
          "file_path": "/results/exec_abc123/node_1_image.png",
          "thumbnail_url": "data:image/jpeg;base64,..."
        }
      }
    },
    "node_2": {
      "status": "completed",
      "outputs": {
        "image": {
          "type": "image",
          "format": "png",
          "width": 1024,
          "height": 1024,
          "data_url": "data:image/png;base64,...",
          "file_path": "/results/exec_abc123/node_2_image.png",
          "thumbnail_url": "data:image/jpeg;base64,..."
        }
      }
    }
  },
  "errors": []
}
```

### 3.4 Preview Node 配置

```json
{
  "preview_node_id": "preview_1",
  "display_name": "Preview",
  "source_connection": {
    "node_id": "node_1",
    "output_name": "image"
  },
  "preview_config": {
    "thumbnail_size": [128, 128],
    "auto_refresh": true,
    "show_metadata": true
  },
  "position": {
    "x": 400,
    "y": 200
  }
}
```

---

## 接口设计

### 4.1 IGraphService 接口

```cpp
// services/IGraphService.h
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

namespace rbc {

/**
 * 执行状态枚举
 */
enum class ExecutionStatus {
    Pending,      // 等待执行
    Running,      // 正在执行
    Completed,    // 执行完成
    Failed,       // 执行失败
    Cancelled     // 已取消
};

/**
 * 执行进度信息
 */
struct ExecutionProgress {
    QString executionId;
    ExecutionStatus status;
    int totalNodes;
    int completedNodes;
    QString currentNodeId;
    QString currentNodeName;
    double progressPercent;
    QString message;
};

/**
 * 节点图服务接口
 */
class IGraphService : public QObject {
    Q_OBJECT
public:
    virtual ~IGraphService() = default;
    
    // === 连接管理 ===
    
    /**
     * 连接到 Remote Server
     * @param serverUrl 服务器地址
     */
    virtual void connectToServer(const QString& serverUrl) = 0;
    
    /**
     * 断开连接
     */
    virtual void disconnectFromServer() = 0;
    
    /**
     * 获取连接状态
     */
    virtual bool isConnected() const = 0;
    
    /**
     * 获取当前服务器地址
     */
    virtual QString serverUrl() const = 0;
    
    // === 节点定义管理 ===
    
    /**
     * 刷新节点定义列表
     */
    virtual void refreshNodeDefinitions() = 0;
    
    /**
     * 获取所有可用的节点类型
     */
    virtual QJsonArray getNodeDefinitions() const = 0;
    
    /**
     * 获取指定节点类型的定义
     */
    virtual QJsonObject getNodeDefinition(const QString& nodeType) const = 0;
    
    /**
     * 按分类获取节点定义
     */
    virtual QMap<QString, QJsonArray> getNodesByCategory() const = 0;
    
    // === 图管理 ===
    
    /**
     * 获取当前活动的图 ID
     */
    virtual QString currentGraphId() const = 0;
    
    /**
     * 切换到指定的图
     */
    virtual bool switchToGraph(const QString& graphId) = 0;
    
    /**
     * 创建新图
     */
    virtual QString createGraph(const QString& name = QString()) = 0;
    
    /**
     * 关闭图
     */
    virtual bool closeGraph(const QString& graphId) = 0;
    
    /**
     * 获取图定义（用于保存）
     */
    virtual QJsonObject getGraphDefinition(const QString& graphId) const = 0;
    
    /**
     * 加载图定义
     */
    virtual bool loadGraphDefinition(const QString& graphId, const QJsonObject& definition) = 0;
    
    // === 执行管理 ===
    
    /**
     * 执行当前图
     * @return 执行 ID
     */
    virtual QString executeCurrentGraph() = 0;
    
    /**
     * 执行指定图
     * @param graphId 图 ID
     * @return 执行 ID
     */
    virtual QString executeGraph(const QString& graphId) = 0;
    
    /**
     * 取消执行
     */
    virtual void cancelExecution(const QString& executionId) = 0;
    
    /**
     * 获取执行状态
     */
    virtual ExecutionStatus getExecutionStatus(const QString& executionId) const = 0;
    
    /**
     * 获取执行进度
     */
    virtual ExecutionProgress getExecutionProgress(const QString& executionId) const = 0;
    
    /**
     * 获取所有活动的执行
     */
    virtual QStringList getActiveExecutions() const = 0;
    
signals:
    // === 连接信号 ===
    void connectionStatusChanged(bool connected);
    void connectionError(const QString& error);
    
    // === 节点定义信号 ===
    void nodeDefinitionsUpdated();
    void nodeDefinitionsLoadFailed(const QString& error);
    
    // === 图信号 ===
    void graphCreated(const QString& graphId);
    void graphClosed(const QString& graphId);
    void currentGraphChanged(const QString& graphId);
    void graphModified(const QString& graphId);
    
    // === 执行信号 ===
    void executionStarted(const QString& executionId, const QString& graphId);
    void executionProgress(const ExecutionProgress& progress);
    void executionCompleted(const QString& executionId);
    void executionFailed(const QString& executionId, const QString& error);
    void executionCancelled(const QString& executionId);
    
    // === 节点执行信号 ===
    void nodeExecutionStarted(const QString& executionId, const QString& nodeId);
    void nodeExecutionCompleted(const QString& executionId, const QString& nodeId);
    void nodeExecutionFailed(const QString& executionId, const QString& nodeId, const QString& error);
};

} // namespace rbc
```

### 4.2 IResultService 接口

```cpp
// services/IResultService.h
#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QPixmap>
#include <QImage>
#include <memory>

namespace rbc {

/**
 * 结果类型枚举
 */
enum class ResultType {
    Image,        // 图片
    Text,         // 文本
    Scene,        // 3D 场景（未来支持）
    Animation,    // 动画序列（未来支持）
    Data,         // 通用数据
    Unknown
};

/**
 * 单个输出结果
 */
struct OutputResult {
    QString resultId;           // 结果唯一 ID
    QString executionId;        // 所属执行 ID
    QString nodeId;             // 产生结果的节点 ID
    QString outputName;         // 输出端口名称
    ResultType type;            // 结果类型
    QJsonObject metadata;       // 元数据（尺寸、格式等）
    QString filePath;           // 文件路径（如果已保存）
    QByteArray data;            // 原始数据（如果在内存中）
    QImage thumbnail;           // 缩略图
    bool hasThumbnail;          // 是否有缩略图
    qint64 timestamp;           // 创建时间戳
};

/**
 * 结果查询选项
 */
struct ResultQuery {
    QString executionId;        // 按执行 ID 查询
    QString nodeId;             // 按节点 ID 查询
    QString outputName;         // 按输出名称查询
    ResultType type;            // 按类型查询
    int limit = -1;             // 返回数量限制
    bool latestOnly = false;    // 仅返回最新结果
};

/**
 * 结果服务接口
 */
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
    virtual QString storeResult(const QString& executionId,
                                const QString& nodeId,
                                const QString& outputName,
                                const QJsonObject& resultData) = 0;
    
    /**
     * 批量存储执行结果
     */
    virtual void storeExecutionResults(const QString& executionId,
                                       const QJsonObject& allResults) = 0;
    
    // === 结果查询 ===
    
    /**
     * 获取单个结果
     */
    virtual std::shared_ptr<OutputResult> getResult(const QString& resultId) const = 0;
    
    /**
     * 按查询条件获取结果列表
     */
    virtual QList<std::shared_ptr<OutputResult>> queryResults(const ResultQuery& query) const = 0;
    
    /**
     * 获取节点的最新输出结果
     */
    virtual std::shared_ptr<OutputResult> getLatestNodeOutput(const QString& nodeId,
                                                               const QString& outputName) const = 0;
    
    /**
     * 获取执行的所有结果
     */
    virtual QList<std::shared_ptr<OutputResult>> getExecutionResults(const QString& executionId) const = 0;
    
    // === 缩略图管理 ===
    
    /**
     * 获取结果的缩略图
     * @param resultId 结果 ID
     * @param maxSize 最大尺寸
     * @return 缩略图图像
     */
    virtual QImage getThumbnail(const QString& resultId, 
                                const QSize& maxSize = QSize(128, 128)) const = 0;
    
    /**
     * 异步生成缩略图
     */
    virtual void generateThumbnailAsync(const QString& resultId,
                                        const QSize& maxSize = QSize(128, 128)) = 0;
    
    // === 结果数据访问 ===
    
    /**
     * 获取完整的图片数据
     */
    virtual QImage getImageData(const QString& resultId) const = 0;
    
    /**
     * 获取文本数据
     */
    virtual QString getTextData(const QString& resultId) const = 0;
    
    /**
     * 获取原始数据
     */
    virtual QByteArray getRawData(const QString& resultId) const = 0;
    
    // === 结果导出 ===
    
    /**
     * 保存结果到文件
     */
    virtual bool saveResultToFile(const QString& resultId, 
                                  const QString& filePath) const = 0;
    
    /**
     * 复制结果到剪贴板
     */
    virtual bool copyResultToClipboard(const QString& resultId) const = 0;
    
    // === 生命周期管理 ===
    
    /**
     * 清除指定执行的所有结果
     */
    virtual void clearExecutionResults(const QString& executionId) = 0;
    
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
    void resultAvailable(const QString& resultId, 
                         const QString& executionId,
                         const QString& nodeId,
                         const QString& outputName);
    
    /**
     * 缩略图生成完成
     */
    void thumbnailReady(const QString& resultId);
    
    /**
     * 结果被清除
     */
    void resultCleared(const QString& resultId);
    
    /**
     * 执行结果全部到达
     */
    void executionResultsComplete(const QString& executionId);
    
    /**
     * 缓存大小变化
     */
    void cacheSizeChanged(qint64 currentSize, qint64 maxSize);
};

} // namespace rbc
```

### 4.3 GraphService 实现类

```cpp
// services/GraphService.h
#pragma once
#include "RBCEditorRuntime/services/IGraphService.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include <QMap>
#include <QTimer>

namespace rbc {

class GraphService : public IGraphService {
    Q_OBJECT
    
public:
    explicit GraphService(QObject* parent = nullptr);
    ~GraphService() override;
    
    // === IGraphService 接口实现 ===
    
    void connectToServer(const QString& serverUrl) override;
    void disconnectFromServer() override;
    bool isConnected() const override;
    QString serverUrl() const override;
    
    void refreshNodeDefinitions() override;
    QJsonArray getNodeDefinitions() const override;
    QJsonObject getNodeDefinition(const QString& nodeType) const override;
    QMap<QString, QJsonArray> getNodesByCategory() const override;
    
    QString currentGraphId() const override;
    bool switchToGraph(const QString& graphId) override;
    QString createGraph(const QString& name = QString()) override;
    bool closeGraph(const QString& graphId) override;
    QJsonObject getGraphDefinition(const QString& graphId) const override;
    bool loadGraphDefinition(const QString& graphId, const QJsonObject& definition) override;
    
    QString executeCurrentGraph() override;
    QString executeGraph(const QString& graphId) override;
    void cancelExecution(const QString& executionId) override;
    ExecutionStatus getExecutionStatus(const QString& executionId) const override;
    ExecutionProgress getExecutionProgress(const QString& executionId) const override;
    QStringList getActiveExecutions() const override;
    
    // === 额外公共方法 ===
    
    /**
     * 设置 ResultService 引用（用于存储执行结果）
     */
    void setResultService(IResultService* resultService);
    
    /**
     * 获取 HttpClient（供其他组件复用连接）
     */
    HttpClient* httpClient() const { return httpClient_; }
    
private slots:
    void onConnectionStatusChanged(bool connected);
    void onNodesLoaded(const QJsonArray& nodes, bool success);
    void onExecutionCompleted(const QString& executionId, const QJsonObject& results);
    void onExecutionError(const QString& executionId, const QString& error);
    void onProgressUpdate(const QString& executionId, const QJsonObject& progress);
    
private:
    void setupHttpClient();
    void startProgressPolling(const QString& executionId);
    void stopProgressPolling(const QString& executionId);
    QString generateExecutionId() const;
    QString generateGraphId() const;
    QJsonObject buildExecutionRequest(const QString& graphId) const;
    
private:
    HttpClient* httpClient_;
    IResultService* resultService_;
    
    // 连接状态
    bool isConnected_;
    QString serverUrl_;
    
    // 节点定义
    QJsonArray nodeDefinitions_;
    QMap<QString, QJsonObject> nodeDefinitionMap_;  // nodeType -> definition
    
    // 图管理
    QString currentGraphId_;
    QMap<QString, QJsonObject> graphs_;  // graphId -> graph definition
    
    // 执行管理
    QMap<QString, ExecutionProgress> activeExecutions_;  // executionId -> progress
    QMap<QString, QTimer*> progressPollingTimers_;  // executionId -> polling timer
    
    // 配置
    int progressPollingInterval_ = 500;  // ms
};

} // namespace rbc
```

### 4.4 ResultService 实现类

```cpp
// services/ResultService.h
#pragma once
#include "RBCEditorRuntime/services/IResultService.h"
#include <QMap>
#include <QMutex>
#include <QCache>
#include <QThread>

namespace rbc {

/**
 * 缩略图生成工作线程
 */
class ThumbnailWorker : public QObject {
    Q_OBJECT
public:
    void generateThumbnail(const QString& resultId, 
                           const QByteArray& imageData,
                           const QSize& maxSize);
signals:
    void thumbnailGenerated(const QString& resultId, const QImage& thumbnail);
};

/**
 * 结果服务实现
 */
class ResultService : public IResultService {
    Q_OBJECT
    
public:
    explicit ResultService(QObject* parent = nullptr);
    ~ResultService() override;
    
    // === IResultService 接口实现 ===
    
    QString storeResult(const QString& executionId,
                        const QString& nodeId,
                        const QString& outputName,
                        const QJsonObject& resultData) override;
    
    void storeExecutionResults(const QString& executionId,
                               const QJsonObject& allResults) override;
    
    std::shared_ptr<OutputResult> getResult(const QString& resultId) const override;
    QList<std::shared_ptr<OutputResult>> queryResults(const ResultQuery& query) const override;
    std::shared_ptr<OutputResult> getLatestNodeOutput(const QString& nodeId,
                                                       const QString& outputName) const override;
    QList<std::shared_ptr<OutputResult>> getExecutionResults(const QString& executionId) const override;
    
    QImage getThumbnail(const QString& resultId, 
                        const QSize& maxSize = QSize(128, 128)) const override;
    void generateThumbnailAsync(const QString& resultId,
                                const QSize& maxSize = QSize(128, 128)) override;
    
    QImage getImageData(const QString& resultId) const override;
    QString getTextData(const QString& resultId) const override;
    QByteArray getRawData(const QString& resultId) const override;
    
    bool saveResultToFile(const QString& resultId, 
                          const QString& filePath) const override;
    bool copyResultToClipboard(const QString& resultId) const override;
    
    void clearExecutionResults(const QString& executionId) override;
    void clearExpiredResults(qint64 olderThanSeconds) override;
    void clearAllResults() override;
    qint64 getCacheSize() const override;
    void setMaxCacheSize(qint64 bytes) override;
    
private:
    QString generateResultId() const;
    ResultType parseResultType(const QString& typeString) const;
    QImage decodeImageFromBase64(const QString& base64Data) const;
    QImage decodeImageFromFile(const QString& filePath) const;
    void updateCacheSize();
    
private slots:
    void onThumbnailGenerated(const QString& resultId, const QImage& thumbnail);
    
private:
    // 结果存储
    mutable QMutex resultsMutex_;
    QMap<QString, std::shared_ptr<OutputResult>> results_;  // resultId -> OutputResult
    
    // 索引结构（用于快速查询）
    QMap<QString, QStringList> executionIndex_;  // executionId -> [resultIds]
    QMap<QString, QStringList> nodeOutputIndex_; // "nodeId:outputName" -> [resultIds] (按时间排序)
    
    // 缓存管理
    QCache<QString, QImage> imageCache_;    // 完整图片缓存
    QCache<QString, QImage> thumbnailCache_; // 缩略图缓存
    qint64 maxCacheSize_;
    qint64 currentCacheSize_;
    
    // 异步缩略图生成
    QThread* thumbnailThread_;
    ThumbnailWorker* thumbnailWorker_;
};

} // namespace rbc
```

---

## Preview Node 设计

### 5.1 概述

Preview Node 是一种编辑器内专用的特殊节点，具有以下特点：

1. **编辑器专用**：不在 Python 节点图定义中存在，仅作为编辑器的可视化辅助
2. **任意连接**：可以连接到任何节点的任何输出端口
3. **实时预览**：自动订阅连接节点的执行结果，显示缩略图
4. **交互式查看**：双击打开详细预览窗口

### 5.2 PreviewNode 类设计

```cpp
// nodes/PreviewNodeModel.h
#pragma once
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeData>
#include <QLabel>
#include <QPixmap>
#include <QWidget>
#include <QVBoxLayout>
#include <memory>

namespace rbc {

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;

class IResultService;
class PreviewWindow;

/**
 * Preview Node - 编辑器专用的预览节点
 * 
 * 特点：
 * 1. 不会被序列化到 Python 执行图中
 * 2. 可以连接到任意输出端口
 * 3. 显示缩略图预览
 * 4. 双击打开详细预览窗口
 */
class PreviewNodeModel : public NodeDelegateModel {
    Q_OBJECT
    
public:
    static constexpr const char* NODE_TYPE = "__editor_preview__";
    static constexpr const char* DISPLAY_NAME = "Preview";
    static constexpr const char* CATEGORY = "Editor/Preview";
    
    explicit PreviewNodeModel();
    ~PreviewNodeModel() override;
    
    // === NodeDelegateModel 接口 ===
    
    [[nodiscard]] QString caption() const override { return DISPLAY_NAME; }
    [[nodiscard]] QString name() const override { return NODE_TYPE; }
    [[nodiscard]] bool captionVisible() const override { return true; }
    
    [[nodiscard]] unsigned int nPorts(PortType portType) const override;
    [[nodiscard]] NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    [[nodiscard]] QString portCaption(PortType portType, PortIndex portIndex) const override;
    [[nodiscard]] bool portCaptionVisible(PortType, PortIndex) const override { return true; }
    
    std::shared_ptr<NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;
    
    QWidget* embeddedWidget() override;
    
    [[nodiscard]] QJsonObject save() const override;
    void load(QJsonObject const& p) override;
    
    // === Preview 特有方法 ===
    
    /**
     * 设置 ResultService 引用
     */
    void setResultService(IResultService* resultService);
    
    /**
     * 获取连接的源节点信息
     */
    QString sourceNodeId() const { return sourceNodeId_; }
    QString sourceOutputName() const { return sourceOutputName_; }
    
    /**
     * 设置源节点信息（当连接建立时调用）
     */
    void setSourceNode(const QString& nodeId, const QString& outputName);
    
    /**
     * 更新预览内容
     */
    void updatePreview();
    
    /**
     * 打开预览窗口
     */
    void openPreviewWindow();
    
    /**
     * 获取当前结果 ID
     */
    QString currentResultId() const { return currentResultId_; }
    
    /**
     * 是否是编辑器专用节点（用于序列化时排除）
     */
    static bool isEditorOnlyNode() { return true; }
    
signals:
    /**
     * 请求打开预览窗口
     */
    void previewWindowRequested(const QString& resultId);
    
protected:
    /**
     * 处理双击事件
     */
    bool eventFilter(QObject* object, QEvent* event) override;
    
private slots:
    void onResultAvailable(const QString& resultId, 
                           const QString& executionId,
                           const QString& nodeId,
                           const QString& outputName);
    void onThumbnailReady(const QString& resultId);
    
private:
    void setupWidget();
    void updateThumbnailDisplay();
    void showPlaceholder();
    void showLoadingIndicator();
    void showThumbnail(const QImage& image);
    void showTextPreview(const QString& text);
    
private:
    IResultService* resultService_;
    
    // 源节点信息
    QString sourceNodeId_;
    QString sourceOutputName_;
    QString currentResultId_;
    
    // UI 组件
    QWidget* mainWidget_;
    QLabel* thumbnailLabel_;
    QLabel* statusLabel_;
    QLabel* typeLabel_;
    
    // 预览配置
    QSize thumbnailSize_;
    bool autoRefresh_;
    
    // 预览窗口
    PreviewWindow* previewWindow_;
};

} // namespace rbc
```

### 5.3 PreviewWindow 预览窗口

```cpp
// ui/PreviewWindow.h
#pragma once
#include <QDialog>
#include <QStackedWidget>
#include <QLabel>
#include <QTextEdit>
#include <QToolBar>
#include <QScrollArea>

namespace rbc {

class IResultService;

/**
 * 预览窗口 - 用于详细查看结果
 * 
 * 支持的预览类型：
 * - 图片：可缩放、可拖拽查看
 * - 文本：带语法高亮的文本查看器
 * - 场景：3D 场景预览（未来支持）
 * - 动画：动画序列播放器（未来支持）
 */
class PreviewWindow : public QDialog {
    Q_OBJECT
    
public:
    explicit PreviewWindow(QWidget* parent = nullptr);
    ~PreviewWindow() override;
    
    /**
     * 设置 ResultService
     */
    void setResultService(IResultService* resultService);
    
    /**
     * 设置要预览的结果
     */
    void setResultId(const QString& resultId);
    
    /**
     * 获取当前预览的结果 ID
     */
    QString resultId() const { return resultId_; }
    
public slots:
    /**
     * 刷新预览内容
     */
    void refresh();
    
    /**
     * 保存结果到文件
     */
    void saveToFile();
    
    /**
     * 复制到剪贴板
     */
    void copyToClipboard();
    
    /**
     * 缩放到适合窗口
     */
    void zoomToFit();
    
    /**
     * 原始大小
     */
    void zoomToActual();
    
    /**
     * 放大
     */
    void zoomIn();
    
    /**
     * 缩小
     */
    void zoomOut();
    
private:
    void setupUi();
    void setupImageViewer();
    void setupTextViewer();
    void setupToolbar();
    
    void showImagePreview(const QImage& image);
    void showTextPreview(const QString& text);
    void showErrorMessage(const QString& message);
    void updateWindowTitle();
    
private:
    IResultService* resultService_;
    QString resultId_;
    
    // UI 组件
    QToolBar* toolbar_;
    QStackedWidget* stackedWidget_;
    
    // 图片预览
    QScrollArea* imageScrollArea_;
    QLabel* imageLabel_;
    double currentZoom_;
    
    // 文本预览
    QTextEdit* textEdit_;
    
    // 错误/占位
    QLabel* placeholderLabel_;
    
    // 元数据显示
    QLabel* metadataLabel_;
};

} // namespace rbc
```

### 5.4 预览窗口样式示例

```qml
// PreviewWindow.qml (如果使用 QML 实现)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: previewWindow
    title: "Result Preview"
    width: 800
    height: 600
    color: "#1e1e1e"
    
    property string resultId: ""
    property var resultService: null
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // 工具栏
        ToolBar {
            Layout.fillWidth: true
            background: Rectangle { color: "#2d2d2d" }
            
            RowLayout {
                anchors.fill: parent
                spacing: 4
                
                ToolButton {
                    icon.source: "qrc:/icons/zoom_fit.svg"
                    ToolTip.text: "Zoom to Fit"
                    onClicked: imageViewer.zoomToFit()
                }
                
                ToolButton {
                    icon.source: "qrc:/icons/zoom_actual.svg"
                    ToolTip.text: "Actual Size"
                    onClicked: imageViewer.zoomToActual()
                }
                
                ToolButton {
                    icon.source: "qrc:/icons/zoom_in.svg"
                    ToolTip.text: "Zoom In"
                    onClicked: imageViewer.zoomIn()
                }
                
                ToolButton {
                    icon.source: "qrc:/icons/zoom_out.svg"
                    ToolTip.text: "Zoom Out"
                    onClicked: imageViewer.zoomOut()
                }
                
                Item { Layout.fillWidth: true }
                
                Label {
                    id: zoomLabel
                    text: Math.round(imageViewer.zoom * 100) + "%"
                    color: "#cccccc"
                }
                
                ToolSeparator {}
                
                ToolButton {
                    icon.source: "qrc:/icons/save.svg"
                    ToolTip.text: "Save to File"
                    onClicked: saveDialog.open()
                }
                
                ToolButton {
                    icon.source: "qrc:/icons/copy.svg"
                    ToolTip.text: "Copy to Clipboard"
                    onClicked: resultService.copyResultToClipboard(resultId)
                }
            }
        }
        
        // 预览区域
        StackLayout {
            id: previewStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0
            
            // 图片预览
            ImageViewer {
                id: imageViewer
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                // 从 ResultService 加载图片
                Component.onCompleted: {
                    if (resultService && resultId) {
                        source = resultService.getImageData(resultId)
                    }
                }
            }
            
            // 文本预览
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                TextArea {
                    id: textViewer
                    readOnly: true
                    font.family: "JetBrains Mono"
                    font.pixelSize: 13
                    color: "#d4d4d4"
                    background: Rectangle { color: "#1e1e1e" }
                    wrapMode: TextEdit.Wrap
                }
            }
            
            // 占位/加载
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                Label {
                    anchors.centerIn: parent
                    text: "No preview available"
                    color: "#808080"
                    font.pixelSize: 16
                }
            }
        }
        
        // 元数据栏
        Rectangle {
            Layout.fillWidth: true
            height: 24
            color: "#252526"
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 16
                
                Label {
                    id: typeLabel
                    text: "Type: Image"
                    color: "#808080"
                    font.pixelSize: 11
                }
                
                Label {
                    id: sizeLabel
                    text: "Size: 512 x 512"
                    color: "#808080"
                    font.pixelSize: 11
                }
                
                Label {
                    id: formatLabel
                    text: "Format: PNG"
                    color: "#808080"
                    font.pixelSize: 11
                }
                
                Item { Layout.fillWidth: true }
                
                Label {
                    id: timestampLabel
                    text: "Generated: 14:30:45"
                    color: "#808080"
                    font.pixelSize: 11
                }
            }
        }
    }
}
```

### 5.5 Preview Node 缩略图 Widget

```cpp
// nodes/PreviewThumbnailWidget.h
#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>

namespace rbc {

/**
 * Preview Node 的嵌入式缩略图 Widget
 * 
 * 特点：
 * 1. 固定大小的缩略图显示区域
 * 2. 加载/错误状态指示
 * 3. 双击打开预览窗口
 * 4. 支持拖拽结果
 */
class PreviewThumbnailWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit PreviewThumbnailWidget(QWidget* parent = nullptr);
    
    /**
     * 显示缩略图
     */
    void showThumbnail(const QImage& image);
    
    /**
     * 显示文本预览（前几行）
     */
    void showTextPreview(const QString& text, int maxLines = 4);
    
    /**
     * 显示加载状态
     */
    void showLoading();
    
    /**
     * 显示占位符
     */
    void showPlaceholder(const QString& message = "Connect to preview");
    
    /**
     * 显示错误
     */
    void showError(const QString& error);
    
    /**
     * 设置结果类型图标
     */
    void setResultTypeIcon(const QString& iconPath);
    
signals:
    /**
     * 双击信号
     */
    void doubleClicked();
    
    /**
     * 拖拽开始
     */
    void dragStarted();
    
protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    
private:
    void setupUi();
    
private:
    QLabel* thumbnailLabel_;
    QLabel* typeIconLabel_;
    QLabel* statusLabel_;
    
    QPoint dragStartPos_;
    bool isDragging_;
    
    static constexpr int THUMBNAIL_SIZE = 128;
    static constexpr int WIDGET_PADDING = 8;
};

} // namespace rbc
```

---

## 实现细节

### 6.1 节点图执行流程

```cpp
QString GraphService::executeGraph(const QString& graphId) {
    if (!isConnected_) {
        emit executionFailed("", "Not connected to server");
        return QString();
    }
    
    // 1. 生成执行 ID
    QString executionId = generateExecutionId();
    
    // 2. 构建执行请求
    QJsonObject request = buildExecutionRequest(graphId);
    request["execution_id"] = executionId;
    
    // 3. 初始化执行状态
    ExecutionProgress progress;
    progress.executionId = executionId;
    progress.status = ExecutionStatus::Pending;
    progress.totalNodes = request["nodes"].toArray().size();
    progress.completedNodes = 0;
    progress.progressPercent = 0.0;
    activeExecutions_[executionId] = progress;
    
    // 4. 发送执行请求
    httpClient_->executeGraph(request, [this, executionId](const QString& resultId, bool success) {
        if (success) {
            // 更新状态为运行中
            if (activeExecutions_.contains(executionId)) {
                activeExecutions_[executionId].status = ExecutionStatus::Running;
                emit executionStarted(executionId, currentGraphId_);
                
                // 启动进度轮询
                startProgressPolling(executionId);
            }
        } else {
            // 执行失败
            if (activeExecutions_.contains(executionId)) {
                activeExecutions_[executionId].status = ExecutionStatus::Failed;
                emit executionFailed(executionId, "Failed to start execution");
            }
        }
    });
    
    return executionId;
}

void GraphService::onExecutionCompleted(const QString& executionId, const QJsonObject& results) {
    // 停止进度轮询
    stopProgressPolling(executionId);
    
    // 更新执行状态
    if (activeExecutions_.contains(executionId)) {
        activeExecutions_[executionId].status = ExecutionStatus::Completed;
        activeExecutions_[executionId].progressPercent = 100.0;
    }
    
    // 将结果存储到 ResultService
    if (resultService_) {
        resultService_->storeExecutionResults(executionId, results);
    }
    
    // 发出完成信号
    emit executionCompleted(executionId);
}
```

### 6.2 结果存储和查询

```cpp
QString ResultService::storeResult(const QString& executionId,
                                   const QString& nodeId,
                                   const QString& outputName,
                                   const QJsonObject& resultData) {
    QMutexLocker locker(&resultsMutex_);
    
    // 1. 生成结果 ID
    QString resultId = generateResultId();
    
    // 2. 解析结果数据
    auto result = std::make_shared<OutputResult>();
    result->resultId = resultId;
    result->executionId = executionId;
    result->nodeId = nodeId;
    result->outputName = outputName;
    result->type = parseResultType(resultData["type"].toString());
    result->metadata = resultData;
    result->timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // 3. 处理不同类型的数据
    if (result->type == ResultType::Image) {
        // 解码 base64 图片数据
        QString dataUrl = resultData["data_url"].toString();
        if (dataUrl.startsWith("data:image")) {
            QString base64 = dataUrl.split(",").last();
            result->data = QByteArray::fromBase64(base64.toLatin1());
        }
        
        // 解码或下载缩略图
        QString thumbnailUrl = resultData["thumbnail_url"].toString();
        if (!thumbnailUrl.isEmpty()) {
            result->thumbnail = decodeImageFromBase64(thumbnailUrl.split(",").last());
            result->hasThumbnail = !result->thumbnail.isNull();
        }
        
        // 保存文件路径
        result->filePath = resultData["file_path"].toString();
    } else if (result->type == ResultType::Text) {
        // 文本直接存储
        result->data = resultData["content"].toString().toUtf8();
    }
    
    // 4. 存储结果
    results_[resultId] = result;
    
    // 5. 更新索引
    executionIndex_[executionId].append(resultId);
    QString indexKey = QString("%1:%2").arg(nodeId, outputName);
    nodeOutputIndex_[indexKey].append(resultId);
    
    // 6. 更新缓存大小
    updateCacheSize();
    
    // 7. 发出信号
    locker.unlock();
    emit resultAvailable(resultId, executionId, nodeId, outputName);
    
    return resultId;
}

std::shared_ptr<OutputResult> ResultService::getLatestNodeOutput(
    const QString& nodeId,
    const QString& outputName) const {
    
    QMutexLocker locker(&resultsMutex_);
    
    QString indexKey = QString("%1:%2").arg(nodeId, outputName);
    
    if (!nodeOutputIndex_.contains(indexKey)) {
        return nullptr;
    }
    
    const QStringList& resultIds = nodeOutputIndex_[indexKey];
    if (resultIds.isEmpty()) {
        return nullptr;
    }
    
    // 返回最新的结果（列表末尾）
    QString latestResultId = resultIds.last();
    return results_.value(latestResultId);
}
```

### 6.3 Preview Node 结果订阅

```cpp
void PreviewNodeModel::setSourceNode(const QString& nodeId, const QString& outputName) {
    sourceNodeId_ = nodeId;
    sourceOutputName_ = outputName;
    
    // 订阅结果更新
    if (resultService_) {
        // 断开旧连接
        disconnect(resultService_, nullptr, this, nullptr);
        
        // 连接新信号
        connect(resultService_, &IResultService::resultAvailable,
                this, &PreviewNodeModel::onResultAvailable);
        connect(resultService_, &IResultService::thumbnailReady,
                this, &PreviewNodeModel::onThumbnailReady);
        
        // 检查是否已有结果
        updatePreview();
    }
}

void PreviewNodeModel::onResultAvailable(const QString& resultId,
                                         const QString& executionId,
                                         const QString& nodeId,
                                         const QString& outputName) {
    // 检查是否是我们关注的节点输出
    if (nodeId == sourceNodeId_ && outputName == sourceOutputName_) {
        currentResultId_ = resultId;
        
        if (autoRefresh_) {
            updateThumbnailDisplay();
        }
    }
}

void PreviewNodeModel::updateThumbnailDisplay() {
    if (!resultService_ || currentResultId_.isEmpty()) {
        showPlaceholder();
        return;
    }
    
    auto result = resultService_->getResult(currentResultId_);
    if (!result) {
        showPlaceholder();
        return;
    }
    
    switch (result->type) {
        case ResultType::Image:
            if (result->hasThumbnail) {
                showThumbnail(result->thumbnail);
            } else {
                showLoadingIndicator();
                resultService_->generateThumbnailAsync(currentResultId_, thumbnailSize_);
            }
            break;
            
        case ResultType::Text:
            showTextPreview(resultService_->getTextData(currentResultId_));
            break;
            
        default:
            showPlaceholder();
            break;
    }
    
    // 更新类型标签
    typeLabel_->setText(QString("Type: %1").arg(result->metadata["type"].toString()));
}

void PreviewNodeModel::openPreviewWindow() {
    if (currentResultId_.isEmpty()) {
        return;
    }
    
    if (!previewWindow_) {
        previewWindow_ = new PreviewWindow();
        previewWindow_->setResultService(resultService_);
    }
    
    previewWindow_->setResultId(currentResultId_);
    previewWindow_->show();
    previewWindow_->raise();
    previewWindow_->activateWindow();
}
```

### 6.4 双击事件处理

```cpp
bool PreviewNodeModel::eventFilter(QObject* object, QEvent* event) {
    if (object == mainWidget_ && event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            openPreviewWindow();
            return true;
        }
    }
    return NodeDelegateModel::eventFilter(object, event);
}
```

---

## 与 NodeEditor Plugin 集成

### 7.1 注册 Preview Node

```cpp
// NodeEditorPlugin.cpp
void NodeEditorPlugin::registerPreviewNode() {
    auto* graphService = pluginManager_->getService<GraphService>();
    auto* resultService = pluginManager_->getService<ResultService>();
    
    // 创建 Preview Node 的工厂函数
    auto previewNodeCreator = [resultService]() {
        auto node = std::make_unique<PreviewNodeModel>();
        node->setResultService(resultService);
        return node;
    };
    
    // 注册到 NodeFactory
    nodeFactory_->getRegistry()->registerModel<PreviewNodeModel>(
        previewNodeCreator,
        PreviewNodeModel::CATEGORY);
    
    qDebug() << "Preview node registered";
}
```

### 7.2 序列化时排除 Preview Node

```cpp
QJsonObject NodeEditor::serializeGraph() {
    QJsonObject graphDef;
    QJsonArray nodesArray;
    QJsonArray connectionsArray;
    QJsonArray previewNodesArray;  // 单独存储 Preview Node
    
    auto nodeIds = m_graphModel->allNodeIds();
    
    for (const auto& nodeId : nodeIds) {
        auto* delegateModel = m_graphModel->delegateModel<NodeDelegateModel>(nodeId);
        if (!delegateModel) continue;
        
        // 检查是否是 Preview Node
        if (delegateModel->name() == PreviewNodeModel::NODE_TYPE) {
            // Preview Node 单独存储，不发送给 Python
            auto* previewNode = dynamic_cast<PreviewNodeModel*>(delegateModel);
            if (previewNode) {
                QJsonObject previewObj;
                previewObj["preview_node_id"] = QString::number(nodeId);
                previewObj["source_node_id"] = previewNode->sourceNodeId();
                previewObj["source_output"] = previewNode->sourceOutputName();
                previewObj["position"] = m_graphModel->nodeData<QJsonObject>(
                    nodeId, QtNodes::NodeRole::Position);
                previewNodesArray.append(previewObj);
            }
            continue;  // 不加入主节点列表
        }
        
        // 普通节点正常序列化
        auto* dynamicNode = dynamic_cast<DynamicNodeModel*>(delegateModel);
        if (dynamicNode) {
            QJsonObject nodeObj;
            nodeObj["node_id"] = QString::number(nodeId);
            nodeObj["node_type"] = dynamicNode->nodeType();
            nodeObj["inputs"] = dynamicNode->getInputValues();
            nodesArray.append(nodeObj);
        }
    }
    
    // ... 连接序列化 ...
    
    graphDef["nodes"] = nodesArray;
    graphDef["connections"] = connectionsArray;
    graphDef["preview_nodes"] = previewNodesArray;  // 编辑器状态
    graphDef["metadata"] = QJsonObject();
    
    return graphDef;
}
```

### 7.3 执行时传递 Preview 信息

```cpp
QJsonObject GraphService::buildExecutionRequest(const QString& graphId) const {
    QJsonObject request = graphs_[graphId];
    
    // 提取 Preview Node 信息，告诉服务器需要返回哪些中间结果
    QJsonArray previewOutputs;
    QJsonArray previewNodes = request["preview_nodes"].toArray();
    
    for (const auto& previewValue : previewNodes) {
        QJsonObject preview = previewValue.toObject();
        QJsonObject outputRequest;
        outputRequest["node_id"] = preview["source_node_id"];
        outputRequest["output_name"] = preview["source_output"];
        outputRequest["include_thumbnail"] = true;
        outputRequest["thumbnail_size"] = QJsonArray{128, 128};
        previewOutputs.append(outputRequest);
    }
    
    request["requested_outputs"] = previewOutputs;
    
    // 移除 preview_nodes（不需要发给服务器）
    request.remove("preview_nodes");
    
    return request;
}
```

---

## 使用示例

### 8.1 主程序集成

```cpp
// rbc_editor_module.cpp 或 main.cpp
int main(int argc, char* argv[]) {
    // ... 初始化 ...
    
    // 创建核心服务
    auto* graphService = new GraphService(&app);
    auto* resultService = new ResultService(&app);
    
    // 关联服务
    graphService->setResultService(resultService);
    
    // 注册到 PluginManager
    pluginManager.registerService(graphService);
    pluginManager.registerService(resultService);
    
    // ... 加载插件 ...
    
    // 连接到服务器
    graphService->connectToServer("http://127.0.0.1:5555");
    
    // ... 其他初始化 ...
}
```

### 8.2 在 NodeEditor 中使用 Preview Node

```cpp
// 用户通过右键菜单添加 Preview Node
void NodeEditor::onAddPreviewNode() {
    // 创建 Preview Node
    QtNodes::NodeId nodeId = m_graphModel->addNode(PreviewNodeModel::NODE_TYPE);
    
    // 获取 Preview Node 实例
    auto* previewNode = m_graphModel->delegateModel<PreviewNodeModel>(nodeId);
    if (previewNode) {
        // 设置 ResultService
        auto* resultService = pluginManager_->getService<ResultService>();
        previewNode->setResultService(resultService);
    }
    
    // 定位到鼠标位置
    QPointF scenePos = m_view->mapToScene(m_view->mapFromGlobal(QCursor::pos()));
    m_graphModel->setNodeData(nodeId, QtNodes::NodeRole::Position, 
                              QJsonValue(QJsonObject{{"x", scenePos.x()}, {"y", scenePos.y()}}));
}

// 当连接建立时，更新 Preview Node 的源节点信息
void NodeEditor::onConnectionCreated(const QtNodes::ConnectionId& connectionId) {
    QtNodes::NodeId inNodeId = connectionId.inNodeId;
    
    // 检查是否连接到 Preview Node
    auto* previewNode = m_graphModel->delegateModel<PreviewNodeModel>(inNodeId);
    if (previewNode) {
        QtNodes::NodeId outNodeId = connectionId.outNodeId;
        QtNodes::PortIndex outPort = connectionId.outPortIndex;
        
        // 获取源节点信息
        auto* sourceNode = m_graphModel->delegateModel<DynamicNodeModel>(outNodeId);
        if (sourceNode) {
            QString outputName = sourceNode->portCaption(QtNodes::PortType::Out, outPort);
            previewNode->setSourceNode(QString::number(outNodeId), outputName);
        }
    }
}
```

### 8.3 监听执行结果

```cpp
// 在 ViewModel 或 Controller 中
void NodeEditorViewModel::setupResultMonitoring() {
    auto* resultService = pluginManager_->getService<ResultService>();
    
    connect(resultService, &IResultService::resultAvailable,
            this, [this](const QString& resultId, 
                        const QString& executionId,
                        const QString& nodeId,
                        const QString& outputName) {
        qDebug() << "Result available:" << nodeId << outputName;
        
        // 高亮显示有新结果的节点
        highlightNodeWithResult(nodeId);
    });
    
    connect(resultService, &IResultService::executionResultsComplete,
            this, [this](const QString& executionId) {
        qDebug() << "All results received for execution:" << executionId;
        
        // 更新所有 Preview Node
        refreshAllPreviewNodes();
    });
}
```

---

## 未来扩展

### 9.1 场景预览 (Scene Preview)

```cpp
// 未来扩展：3D 场景预览
class ScenePreviewWidget : public QWidget {
public:
    void setSceneData(const QJsonObject& sceneData);
    void setCamera(const QVector3D& position, const QVector3D& target);
    void enableOrbitControls(bool enable);
    
private:
    // 使用轻量级 3D 渲染
    QOpenGLWidget* glWidget_;
    // 或者使用 Viewport 的简化版本
};
```

### 9.2 动画序列预览 (Animation Sequence Preview)

```cpp
// 未来扩展：动画序列预览
class AnimationPreviewWidget : public QWidget {
public:
    void setFrames(const QList<QImage>& frames, int fps);
    void play();
    void pause();
    void stop();
    void seekToFrame(int frameIndex);
    void setLooping(bool loop);
    
signals:
    void frameChanged(int currentFrame);
    void playbackFinished();
    
private:
    QList<QImage> frames_;
    int currentFrame_;
    int fps_;
    QTimer* playbackTimer_;
};
```

### 9.3 结果比较 (Result Comparison)

```cpp
// 未来扩展：多结果对比
class ResultCompareWindow : public QDialog {
public:
    void addResult(const QString& resultId, const QString& label);
    void setCompareMode(CompareMode mode);  // SideBySide, Overlay, Diff
    
private:
    QList<QString> resultIds_;
    CompareMode mode_;
};
```

---

## 配置选项

### 10.1 GraphService 配置

```json
{
  "graph_service": {
    "default_server_url": "http://127.0.0.1:5555",
    "connection_timeout": 5000,
    "execution_timeout": 300000,
    "progress_polling_interval": 500,
    "auto_reconnect": true,
    "reconnect_interval": 3000,
    "max_concurrent_executions": 3
  }
}
```

### 10.2 ResultService 配置

```json
{
  "result_service": {
    "max_cache_size_mb": 512,
    "thumbnail_size": [128, 128],
    "thumbnail_quality": 85,
    "result_expiry_hours": 24,
    "auto_cleanup_interval": 3600,
    "persist_results": true,
    "results_directory": "${project}/intermediate/results"
  }
}
```

### 10.3 Preview Node 配置

```json
{
  "preview_node": {
    "default_thumbnail_size": [128, 128],
    "auto_refresh": true,
    "show_type_indicator": true,
    "double_click_action": "open_window",
    "text_preview_max_lines": 6,
    "text_preview_font_size": 10
  }
}
```

---

## 总结

GraphService 和 ResultService 共同构成了 RoboCute Editor 节点图执行系统的核心：

1. **GraphService** 负责与 Remote Server 通信，管理节点定义、图编辑状态和执行请求
2. **ResultService** 负责存储、查询和管理执行结果，提供缩略图生成和预览支持
3. **Preview Node** 作为编辑器专用的可视化辅助，让用户可以方便地预览节点输出

这三个组件协同工作，提供流畅的节点图编辑和结果预览体验，同时保持了与 Python 后端的清晰边界——Preview Node 仅存在于编辑器端，不会影响实际的节点图执行逻辑。
