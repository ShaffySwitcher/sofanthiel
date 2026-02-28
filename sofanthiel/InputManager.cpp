#include "InputManager.h"

// Common keybind definitions
const Keybind InputManager::New       = { ImGuiKey_N,      true,  false, false, "Ctrl+N" };
const Keybind InputManager::Open      = { ImGuiKey_O,      true,  false, false, "Ctrl+O" };
const Keybind InputManager::Save      = { ImGuiKey_S,      true,  false, false, "Ctrl+S" };
const Keybind InputManager::SaveAs    = { ImGuiKey_S,      true,  true,  false, "Ctrl+Shift+S" };
const Keybind InputManager::Undo      = { ImGuiKey_Z,      true,  false, false, "Ctrl+Z" };
const Keybind InputManager::Redo      = { ImGuiKey_Y,      true,  false, false, "Ctrl+Y" };
const Keybind InputManager::RedoAlt   = { ImGuiKey_Z,      true,  true,  false, "Ctrl+Shift+Z" };
const Keybind InputManager::Copy      = { ImGuiKey_C,      true,  false, false, "Ctrl+C" };
const Keybind InputManager::Paste     = { ImGuiKey_V,      true,  false, false, "Ctrl+V" };
const Keybind InputManager::Cut       = { ImGuiKey_X,      true,  false, false, "Ctrl+X" };
const Keybind InputManager::Delete    = { ImGuiKey_Delete,  false, false, false, "Del" };
const Keybind InputManager::SelectAll = { ImGuiKey_A,      true,  false, false, "Ctrl+A" };
const Keybind InputManager::PlayPause = { ImGuiKey_Space,   false, false, false, "Space" };
const Keybind InputManager::Escape    = { ImGuiKey_Escape,  false, false, false, "Esc" };

bool InputManager::isPressed(const Keybind& bind)
{
    if (!ImGui::IsKeyPressed(bind.key, false))
        return false;

    const ImGuiIO& io = ImGui::GetIO();

    if (bind.ctrl != io.KeyCtrl)
        return false;
    if (bind.shift != io.KeyShift)
        return false;
    if (bind.alt != io.KeyAlt)
        return false;

    return true;
}
