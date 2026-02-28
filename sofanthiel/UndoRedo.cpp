#include "UndoRedo.h"

void UndoRedoManager::execute(std::unique_ptr<UndoableAction> action)
{
    action->execute();
    undoStack.push_back(std::move(action));
    redoStack.clear();

    if (static_cast<int>(undoStack.size()) > MAX_UNDO_HISTORY) {
        undoStack.erase(undoStack.begin());
    }
}

void UndoRedoManager::undo()
{
    if (undoStack.empty()) return;

    auto action = std::move(undoStack.back());
    undoStack.pop_back();

    action->undo();
    redoStack.push_back(std::move(action));
}

void UndoRedoManager::redo()
{
    if (redoStack.empty()) return;

    auto action = std::move(redoStack.back());
    redoStack.pop_back();

    action->redo();
    undoStack.push_back(std::move(action));
}

bool UndoRedoManager::canUndo() const
{
    return !undoStack.empty();
}

bool UndoRedoManager::canRedo() const
{
    return !redoStack.empty();
}

std::string UndoRedoManager::undoDescription() const
{
    if (undoStack.empty()) return "";
    return undoStack.back()->description();
}

std::string UndoRedoManager::redoDescription() const
{
    if (redoStack.empty()) return "";
    return redoStack.back()->description();
}

void UndoRedoManager::clear()
{
    undoStack.clear();
    redoStack.clear();
}
