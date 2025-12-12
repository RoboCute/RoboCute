#pragma once

#include <QObject>
#include <QString>
#include <QUndoStack>
#include <QUndoCommand>
#include <memory>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/EventBus.h"

namespace rbc {

/**
 * 命令基类
 * 所有可撤销的命令都应该继承此类
 */
class RBC_EDITOR_RUNTIME_API EditorCommand : public QUndoCommand {
public:
    explicit EditorCommand(const QString &text, QUndoCommand *parent = nullptr);
    virtual ~EditorCommand() = default;

    /**
     * 执行命令
     */
    virtual void execute() = 0;

    /**
     * 撤销命令
     */
    virtual void undo() override = 0;

    /**
     * 重做命令
     */
    virtual void redo() override;
};

/**
 * 命令总线 - 管理命令队列，支持撤销和重做
 * 
 * 职责：
 * - 管理命令队列
 * - 支持命令的发送、撤销和重做
 * - 与事件总线集成，命令执行时发布事件
 */
class RBC_EDITOR_RUNTIME_API CommandBus : public QObject {
    Q_OBJECT
public:
    CommandBus(const CommandBus &) = delete;
    CommandBus &operator=(const CommandBus &) = delete;

public:
    /**
     * 获取命令总线单例
     */
    static CommandBus &instance();

    /**
     * 执行命令
     * @param command 命令对象（CommandBus会取得所有权）
     */
    void execute(EditorCommand *command);

    /**
     * 撤销上一个命令
     */
    void undo();

    /**
     * 重做上一个撤销的命令
     */
    void redo();

    /**
     * 检查是否可以撤销
     */
    [[nodiscard]] bool canUndo() const;

    /**
     * 检查是否可以重做
     */
    [[nodiscard]] bool canRedo() const;

    /**
     * 获取撤销文本
     */
    [[nodiscard]] QString undoText() const;

    /**
     * 获取重做文本
     */
    [[nodiscard]] QString redoText() const;

    /**
     * 清除命令历史
     */
    void clear();

    /**
     * 设置最大命令历史数量
     */
    void setUndoLimit(int limit);

    /**
     * 获取命令栈（用于高级操作）
     */
    QUndoStack *undoStack() { return &undoStack_; }

signals:
    /**
     * 当命令历史改变时发出（用于更新UI）
     */
    void commandHistoryChanged();

    /**
     * 当命令执行时发出
     */
    void commandExecuted(const QString &commandText);

    /**
     * 当命令撤销时发出
     */
    void commandUndone(const QString &commandText);

    /**
     * 当命令重做时发出
     */
    void commandRedone(const QString &commandText);

private:
    explicit CommandBus(QObject *parent = nullptr);
    ~CommandBus() override = default;

    QUndoStack undoStack_;

private slots:
    void onCommandExecuted();
    void onCommandUndone();
    void onCommandRedone();
};

}// namespace rbc
