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

EditorCommandBus::EditorCommandBus(QObject *parent) : QObject(parent), _undo_stack(this) {
    connect(&_undo_stack, &QUndoStack::indexChanged, this, &EditorCommandBus::commandHistoryChanged);
}

void EditorCommandBus::execute(EditorCommand *command) {
    if (!command) {
        qWarning() << "EditorCommandBus: Attempted to execute null command";
        return;
    }

    _undo_stack.push(command);
    emit commandExecuted(command->text());
}

void EditorCommandBus::undo() {
    if (canUndo()) {
        _undo_stack.undo();
        emit commandUndone(undoText());
    }
}

void EditorCommandBus::redo() {
    if (canRedo()) {
        _undo_stack.redo();
        emit commandRedone(redoText());
    }
}

bool EditorCommandBus::canUndo() const {
    return _undo_stack.canUndo();
}

bool EditorCommandBus::canRedo() const {
    return _undo_stack.canRedo();
}

QString EditorCommandBus::undoText() const {
    return _undo_stack.undoText();
}

QString EditorCommandBus::redoText() const {
    return _undo_stack.redoText();
}

void EditorCommandBus::clear() {
    _undo_stack.clear();
    emit commandHistoryChanged();
}

void EditorCommandBus::setUndoLimit(int limit) {
    _undo_stack.setUndoLimit(limit);
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