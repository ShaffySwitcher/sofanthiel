#pragma once

#include "imgui.h"
#include <functional>
#include <string>
#include <vector>

struct Keybind {
    ImGuiKey key;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    std::string label;  // example "Ctrl+S"
};

class InputManager
{
public:
    static bool isPressed(const Keybind& bind);

    static const Keybind New;
    static const Keybind Open;
    static const Keybind Save;
    static const Keybind SaveAs;
    static const Keybind Undo;
    static const Keybind Redo;
    static const Keybind RedoAlt;  // Ctrl+Shift+Z
    static const Keybind Copy;
    static const Keybind Paste;
    static const Keybind Cut;
    static const Keybind Delete;
    static const Keybind SelectAll;
    static const Keybind PlayPause;
    static const Keybind Escape;
};
