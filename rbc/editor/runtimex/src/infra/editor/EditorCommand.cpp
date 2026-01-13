#include "RBCEditorRuntime/infra/editor/EditorCommand.h"
#include <QDebug>

namespace rbc {

EditorCommand::EditorCommand(const QString &text, QUndoCommand *parent) : QUndoCommand(text, parent) {
}

void EditorCommand::redo() {
    execute();
}

EditorCommandBus &EditorCommandBus::instance() {
    static EditorCommandBus bus;
    return bus;
}

EditorCommandBus::EditorCommandBus(QObject *parent) : QObject(parent), undoStack_(this) {
    connect(&undoStack_, &QUndoStack::indexChanged, this, &EditorCommandBus::commandHistoryChanged);
}

void EditorCommandBus::execute(EditorCommand *command) {
    if (!command) {
        qWarning() << "EditorCommandBus: Attempted to execute null command";
        return;
    }

    undoStack_.push(command);
    emit commandExecuted(command->text());
}

void EditorCommandBus::undo() {
    if (canUndo()) {
        undoStack_.undo();
        emit commandUndone(undoText());
    }
}

void EditorCommandBus::redo() {
    if (canRedo()) {
        undoStack_.redo();
        emit commandRedone(redoText());
    }
}

bool EditorCommandBus::canUndo() const {
    return undoStack_.canUndo();
}

bool EditorCommandBus::canRedo() const {
    return undoStack_.canRedo();
}

QString EditorCommandBus::undoText() const {
    return undoStack_.undoText();
}

QString EditorCommandBus::redoText() const {
    return undoStack_.redoText();
}

void EditorCommandBus::clear() {
    undoStack_.clear();
    emit commandHistoryChanged();
}

void EditorCommandBus::setUndoLimit(int limit) {
    undoStack_.setUndoLimit(limit);
}

void EditorCommandBus::onCommandExecuted() {
    emit commandHistoryChanged();
}

void EditorCommandBus::onCommandUndone() {
    emit commandHistoryChanged();
}

void EditorCommandBus::onCommandRedone() {
    emit commandHistoryChanged();
}

}// namespace rbc