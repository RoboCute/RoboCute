#pragma once

#include <rbc_config.h>
#include <QObject>
#include <QString>
#include <QUndoStack>
#include <QUndoCommand>

namespace rbc {

class RBC_EDITOR_RUNTIME_API EditorCommand : public QUndoCommand {
public:
    explicit EditorCommand(const QString &text, QUndoCommand *parent = nullptr);
    virtual ~EditorCommand() = default;

    virtual void execute() = 0;
    virtual void undo() override = 0;
    virtual void redo() override;
};

class RBC_EDITOR_RUNTIME_API EditorCommandBus : public QObject {
    Q_OBJECT
public:
    EditorCommandBus(const EditorCommandBus &) = delete;
    EditorCommandBus &operator=(const EditorCommandBus &) = delete;

public:
    static EditorCommandBus &instance();
    void execute(EditorCommand *command);
    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;
    void clear();
    void setUndoLimit(int limit);
    QUndoStack *undoStack() { return &undoStack_; }

signals:
    void commandHistoryChanged();
    void commandExecuted(const QString &commandText);
    void commandUndone(const QString &commandText);
    void commandRedone(const QString &commandText);


private:
    explicit EditorCommandBus(QObject *parent = nullptr);
    ~EditorCommandBus() override = default;

    QUndoStack undoStack_;

private slots:
    void onCommandExecuted();
    void onCommandUndone();
    void onCommandRedone();
};

}// namespace rbc