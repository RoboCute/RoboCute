#include "RBCEditorRuntime/runtime/CommandBus.h"
#include <QDebug>

namespace rbc {

EditorCommand::EditorCommand(const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent) {
}

void EditorCommand::redo() {
    execute();
}

CommandBus &CommandBus::instance() {
    static CommandBus bus;
    return bus;
}

CommandBus::CommandBus(QObject *parent)
    : QObject(parent),
      undoStack_(this) {
    // 连接命令栈的信号
    connect(&undoStack_, &QUndoStack::indexChanged, this, &CommandBus::commandHistoryChanged);
}

void CommandBus::execute(EditorCommand *command) {
    if (!command) {
        qWarning() << "CommandBus: Attempted to execute null command";
        return;
    }

    undoStack_.push(command);
    emit commandExecuted(command->text());
}

void CommandBus::undo() {
    if (canUndo()) {
        undoStack_.undo();
        emit commandUndone(undoText());
    }
}

void CommandBus::redo() {
    if (canRedo()) {
        undoStack_.redo();
        emit commandRedone(redoText());
    }
}

bool CommandBus::canUndo() const {
    return undoStack_.canUndo();
}

bool CommandBus::canRedo() const {
    return undoStack_.canRedo();
}

QString CommandBus::undoText() const {
    return undoStack_.undoText();
}

QString CommandBus::redoText() const {
    return undoStack_.redoText();
}

void CommandBus::clear() {
    undoStack_.clear();
    emit commandHistoryChanged();
}

void CommandBus::setUndoLimit(int limit) {
    undoStack_.setUndoLimit(limit);
}

void CommandBus::onCommandExecuted() {
    emit commandHistoryChanged();
}

void CommandBus::onCommandUndone() {
    emit commandHistoryChanged();
}

void CommandBus::onCommandRedone() {
    emit commandHistoryChanged();
}

}// namespace rbc
