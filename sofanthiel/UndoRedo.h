#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

class UndoableAction
{
public:
    virtual ~UndoableAction() = default;

    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual void redo() { execute(); }
    virtual std::string description() const = 0;
};

class UndoRedoManager
{
public:
    void execute(std::unique_ptr<UndoableAction> action);
    void undo();
    void redo();

    bool canUndo() const;
    bool canRedo() const;

    std::string undoDescription() const;
    std::string redoDescription() const;

    void clear();

    int undoCount() const { return static_cast<int>(undoStack.size()); }
    int redoCount() const { return static_cast<int>(redoStack.size()); }

private:
    static constexpr int MAX_UNDO_HISTORY = 100;

    std::vector<std::unique_ptr<UndoableAction>> undoStack;
    std::vector<std::unique_ptr<UndoableAction>> redoStack;
};

class LambdaAction : public UndoableAction
{
public:
    LambdaAction(std::string desc,
                 std::function<void()> doFunc,
                 std::function<void()> undoFunc)
        : desc(std::move(desc))
        , doFunc(std::move(doFunc))
        , undoFunc(std::move(undoFunc))
    {}

    void execute() override { doFunc(); }
    void undo() override { undoFunc(); }
    std::string description() const override { return desc; }

private:
    std::string desc;
    std::function<void()> doFunc;
    std::function<void()> undoFunc;
};

#include "Graphics.h"

class OAMModifyAction : public UndoableAction
{
public:
    OAMModifyAction(std::string desc,
                    std::function<std::vector<TengokuOAM>*()> targetResolver,
                    std::vector<TengokuOAM> oldState,
                    std::vector<TengokuOAM> newState)
        : desc(std::move(desc))
        , targetResolver(std::move(targetResolver))
        , oldState(std::move(oldState))
        , newState(std::move(newState))
    {}

    OAMModifyAction(std::string desc,
                    std::vector<TengokuOAM>* target,
                    std::vector<TengokuOAM> oldState,
                    std::vector<TengokuOAM> newState)
        : OAMModifyAction(
            std::move(desc),
            [target]() { return target; },
            std::move(oldState),
            std::move(newState))
    {}

    void execute() override {
        if (auto* target = targetResolver()) {
            *target = newState;
        }
    }
    void undo() override {
        if (auto* target = targetResolver()) {
            *target = oldState;
        }
    }
    std::string description() const override { return desc; }

private:
    std::string desc;
    std::function<std::vector<TengokuOAM>*()> targetResolver;
    std::vector<TengokuOAM> oldState;
    std::vector<TengokuOAM> newState;
};

class AnimationEntriesAction : public UndoableAction
{
public:
    AnimationEntriesAction(std::string desc,
                           std::function<std::vector<AnimationEntry>*()> targetResolver,
                           std::vector<AnimationEntry> oldState,
                           std::vector<AnimationEntry> newState)
        : desc(std::move(desc))
        , targetResolver(std::move(targetResolver))
        , oldState(std::move(oldState))
        , newState(std::move(newState))
    {}

    AnimationEntriesAction(std::string desc,
                           std::vector<AnimationEntry>* target,
                           std::vector<AnimationEntry> oldState,
                           std::vector<AnimationEntry> newState)
        : AnimationEntriesAction(
            std::move(desc),
            [target]() { return target; },
            std::move(oldState),
            std::move(newState))
    {}

    void execute() override {
        if (auto* target = targetResolver()) {
            *target = newState;
        }
    }
    void undo() override {
        if (auto* target = targetResolver()) {
            *target = oldState;
        }
    }
    std::string description() const override { return desc; }

private:
    std::string desc;
    std::function<std::vector<AnimationEntry>*()> targetResolver;
    std::vector<AnimationEntry> oldState;
    std::vector<AnimationEntry> newState;
};

class PaletteChangeAction : public UndoableAction
{
public:
    PaletteChangeAction(std::string desc,
                        std::function<Palette*()> targetResolver,
                        Palette oldState,
                        Palette newState)
        : desc(std::move(desc))
        , targetResolver(std::move(targetResolver))
        , oldState(oldState)
        , newState(newState)
    {}

    PaletteChangeAction(std::string desc,
                        Palette* target,
                        Palette oldState,
                        Palette newState)
        : PaletteChangeAction(
            std::move(desc),
            [target]() { return target; },
            oldState,
            newState)
    {}

    void execute() override {
        if (auto* target = targetResolver()) {
            *target = newState;
        }
    }
    void undo() override {
        if (auto* target = targetResolver()) {
            *target = oldState;
        }
    }
    std::string description() const override { return desc; }

private:
    std::string desc;
    std::function<Palette*()> targetResolver;
    Palette oldState;
    Palette newState;
};
