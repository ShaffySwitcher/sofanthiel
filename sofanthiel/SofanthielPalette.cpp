#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"

void Sofanthiel::handlePalette()
{
    ImGui::Begin("Palette", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(ICON_FA_PLUS " Add Palette") && palettes.size() < 16) {
        Palette newPalette;
        for (int i = 0; i < 16; i++) {
            Uint8 gray = (Uint8)(i * 255 / 15);
            newPalette.colors[i] = { gray, gray, gray, 255 };
        }
        palettes.push_back(newPalette);
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MINUS " Remove Palette") && palettes.size() > 0) {
        palettes.pop_back();
        currentPalette = SDL_clamp(currentPalette, 0, (int)palettes.size() - 1);
    }

    ImGui::SameLine();
    ImGui::Text("Palettes: %d/16", (int)palettes.size());

    ImGui::Separator();

    float infoBarHeight = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y;
    float contentHeight = ImMax(ImGui::GetContentRegionAvail().y - infoBarHeight, 50.0f);

    ImGui::BeginChild("PaletteContent", ImVec2(0, contentHeight), ImGuiChildFlags_None);

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 baseSize = ImVec2(16 * 16, 16 * palettes.size());
    ImVec2 origin = paletteView.calculateOrigin(contentCenter, baseSize);

    drawPaletteContent(origin);
    handlePaletteRowSelection(origin);
    handlePaletteContextMenu();

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("PaletteInfo", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImVec2 mousePosInWindow = ImVec2(
        ImGui::GetIO().MousePos.x - origin.x,
        ImGui::GetIO().MousePos.y - origin.y
    );

    drawPaletteInfoPanel(paletteView, mousePosInWindow, baseSize, origin);

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::drawPaletteContent(const ImVec2& origin)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float totalWidth = 16.0f * 16 * paletteView.zoom;
    float totalHeight = 16.0f * palettes.size() * paletteView.zoom;

    ImVec2 scaledSize = ImVec2(totalWidth, totalHeight);

    drawBackground(drawList, origin, scaledSize, paletteView.backgroundColor);

    drawPaletteColors(drawList, origin, scaledSize);

    if (!palettes.empty()) {
        drawList->AddRect(
            origin,
            ImVec2(origin.x + scaledSize.x, origin.y + scaledSize.y),
            IM_COL32(100, 100, 100, 255)
        );
    }

    drawPaletteGrid(drawList, origin, scaledSize);

    paletteView.handleZoomAndPan(ImGui::IsWindowHovered(), origin);
}

void Sofanthiel::drawPaletteColors(ImDrawList* drawList, ImVec2 origin, ImVec2 scaledSize)
{
    if (palettes.empty()) {
        ImVec2 textPos = calculateContentCenter();
        textPos.x -= ImGui::CalcTextSize("No palette loaded").x * 0.5f;
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "No palette loaded");
        return;
    }

    const int paletteWidth = 16;
    const int paletteHeight = 16;

    float celSize = 16.0f * paletteView.zoom;

    float totalWidth = celSize * paletteWidth;
    float totalHeight = celSize * palettes.size();

    ImVec2 palettePos = ImVec2(
        origin.x + (scaledSize.x - totalWidth) * 0.5f,
        origin.y + (scaledSize.y - totalHeight) * 0.5f
    );

    for (int i = 0; i < palettes.size(); i++) {
        const Palette& palette = palettes[i];
        for (int j = 0; j < 16; j++) {
            const SDL_Color& color = palette.colors[j];
            int x = j % paletteWidth;
            int y = i;
            ImVec2 celPos = ImVec2(
                palettePos.x + x * celSize,
                palettePos.y + y * celSize
            );
            drawList->AddRectFilled(
                celPos,
                ImVec2(celPos.x + celSize, celPos.y + celSize),
                IM_COL32(color.r, color.g, color.b, 255)
            );

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
                ImVec2 mousePos = ImGui::GetIO().MousePos;
                if (mousePos.x >= celPos.x && mousePos.x < celPos.x + celSize &&
                    mousePos.y >= celPos.y && mousePos.y < celPos.y + celSize) {

                    paletteUndoSnapshot = palettes[i];
                    paletteUndoSnapshotIndex = i;

                    editPaletteIdx = i;
                    editColorIdx = j;
                    editColor[0] = color.r / 255.0f;
                    editColor[1] = color.g / 255.0f;
                    editColor[2] = color.b / 255.0f;
                    editColor[3] = color.a / 255.0f;

                    ImGui::OpenPopup("Color Picker");
                }
            }
        }
    }

    handleColorPicker(editPaletteIdx, editColorIdx, editColor);
}

void Sofanthiel::handleColorPicker(int editPaletteIdx, int editColorIdx, float editColor[4])
{
    if (ImGui::BeginPopup("Color Picker", ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Edit Color (Palette %d, Index %d)", editPaletteIdx, editColorIdx);
        ImGui::Separator();

        if (ImGui::ColorPicker4("##picker", editColor, ImGuiColorEditFlags_AlphaBar)) {
            palettes[editPaletteIdx].colors[editColorIdx].r = static_cast<Uint8>(editColor[0] * 255.0f);
            palettes[editPaletteIdx].colors[editColorIdx].g = static_cast<Uint8>(editColor[1] * 255.0f);
            palettes[editPaletteIdx].colors[editColorIdx].b = static_cast<Uint8>(editColor[2] * 255.0f);
            palettes[editPaletteIdx].colors[editColorIdx].a = static_cast<Uint8>(editColor[3] * 255.0f);
        }

        ImGui::EndPopup();
    }
    else {
        if (paletteUndoSnapshotIndex >= 0 && paletteUndoSnapshotIndex < static_cast<int>(palettes.size())) {
            Palette& current = palettes[paletteUndoSnapshotIndex];
            bool changed = memcmp(&paletteUndoSnapshot, &current, sizeof(Palette)) != 0;
            if (changed) {
                Palette newState = current;
                Palette oldState = paletteUndoSnapshot;
                int idx = paletteUndoSnapshotIndex;
                undoManager.execute(std::make_unique<LambdaAction>(
                    "Change Palette Color",
                    [this, idx, newState]() { if (idx < palettes.size()) palettes[idx] = newState; },
                    [this, idx, oldState]() { if (idx < palettes.size()) palettes[idx] = oldState; }
                ));
            }
            paletteUndoSnapshotIndex = -1;
        }
    }
}

void Sofanthiel::initializeDefaultPalettes()
{
    Palette defaultPalette;

    defaultPalette.colors[0] = { 0, 0, 0, 255 };
    defaultPalette.colors[1] = { 255, 255, 255, 255 };
    defaultPalette.colors[2] = { 255, 0, 0, 255 };
    defaultPalette.colors[3] = { 0, 255, 0, 255 };
    defaultPalette.colors[4] = { 0, 0, 255, 255 };
    defaultPalette.colors[5] = { 255, 255, 0, 255 };
    defaultPalette.colors[6] = { 255, 0, 255, 255 };
    defaultPalette.colors[7] = { 0, 255, 255, 255 };
    defaultPalette.colors[8] = { 128, 128, 128, 255 };
    defaultPalette.colors[9] = { 192, 192, 192, 255 };
    defaultPalette.colors[10] = { 128, 0, 0, 255 };
    defaultPalette.colors[11] = { 0, 128, 0, 255 };
    defaultPalette.colors[12] = { 0, 0, 128, 255 };
    defaultPalette.colors[13] = { 128, 128, 0, 255 };
    defaultPalette.colors[14] = { 128, 0, 128, 255 };
    defaultPalette.colors[15] = { 0, 128, 128, 255 };

    this->palettes.clear();
    this->palettes.push_back(defaultPalette);
}

void Sofanthiel::drawPaletteGrid(ImDrawList* drawList, ImVec2 origin, ImVec2 scaledSize)
{
    if (!showGrid || palettes.empty()) return;

    ImU32 gridColor = IM_COL32(70, 70, 70, 200);

    const int paletteWidth = 16;
    float celSize = 16.0f * paletteView.zoom;

    float usedWidth = celSize * paletteWidth;
    float usedHeight = celSize * palettes.size();

    ImVec2 gridOrigin = ImVec2(
        origin.x + (scaledSize.x - usedWidth) * 0.5f,
        origin.y + (scaledSize.y - usedHeight) * 0.5f
    );

    for (int x = 0; x <= paletteWidth; x++) {
        float xPos = gridOrigin.x + x * celSize;
        drawList->AddLine(
            ImVec2(xPos, gridOrigin.y),
            ImVec2(xPos, gridOrigin.y + usedHeight),
            gridColor
        );
    }

    for (int y = 0; y <= palettes.size(); y++) {
        float yPos = gridOrigin.y + y * celSize;
        drawList->AddLine(
            ImVec2(gridOrigin.x, yPos),
            ImVec2(gridOrigin.x + usedWidth, yPos),
            gridColor
        );
    }
}

void Sofanthiel::handlePaletteRowSelection(const ImVec2& origin)
{
    if (palettes.empty()) return;

    float celSize = 16.0f * paletteView.zoom;
    float rowHeight = celSize;
    float rowWidth = celSize * 16;
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        int row = static_cast<int>(floor((mousePos.y - origin.y) / rowHeight));
        if (row >= 0 && row < static_cast<int>(palettes.size()) &&
            mousePos.x >= origin.x && mousePos.x < origin.x + rowWidth) {
            selectedPaletteRow = row;
        }
    }

    if (selectedPaletteRow >= 0 && selectedPaletteRow < static_cast<int>(palettes.size())) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p0(origin.x, origin.y + selectedPaletteRow * rowHeight);
        ImVec2 p1(origin.x + rowWidth, origin.y + (selectedPaletteRow + 1) * rowHeight);

        drawList->AddRectFilled(p0, p1, IM_COL32(100, 180, 255, 30));
        drawList->AddRect(p0, p1, IM_COL32(100, 180, 255, 220), 0, 0, 2.0f);
    }

    bool canUseShortcuts = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                           selectedPaletteRow >= 0 && selectedPaletteRow < static_cast<int>(palettes.size());

    if (canUseShortcuts) {
        if (InputManager::isPressed(InputManager::Copy)) {
            paletteClipboard = palettes[selectedPaletteRow];
            hasPaletteClipboard = true;
        }

        if (InputManager::isPressed(InputManager::Paste) && hasPaletteClipboard) {
            std::vector<Palette> oldPalettes = palettes;
            palettes[selectedPaletteRow] = paletteClipboard;
            std::vector<Palette> newPalettes = palettes;
            int row = selectedPaletteRow;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Paste Palette Row",
                [this, newPalettes]() { this->palettes = newPalettes; },
                [this, oldPalettes]() { this->palettes = oldPalettes; }
            ));
        }

        if (InputManager::isPressed(InputManager::Delete)) {
            if (palettes.size() > 1) {
                std::vector<Palette> oldPalettes = palettes;
                int oldSelectedRow = selectedPaletteRow;
                int oldCurrentPalette = currentPalette;

                palettes.erase(palettes.begin() + selectedPaletteRow);
                currentPalette = SDL_clamp(currentPalette, 0, static_cast<int>(palettes.size()) - 1);
                if (selectedPaletteRow >= static_cast<int>(palettes.size())) {
                    selectedPaletteRow = static_cast<int>(palettes.size()) - 1;
                }

                std::vector<Palette> newPalettes = palettes;
                int newCurrentPalette = currentPalette;
                int newSelectedRow = selectedPaletteRow;

                undoManager.execute(std::make_unique<LambdaAction>(
                    "Delete Palette Row",
                    [this, newPalettes, newCurrentPalette, newSelectedRow]() {
                        this->palettes = newPalettes;
                        this->currentPalette = newCurrentPalette;
                        this->selectedPaletteRow = newSelectedRow;
                    },
                    [this, oldPalettes, oldCurrentPalette, oldSelectedRow]() {
                        this->palettes = oldPalettes;
                        this->currentPalette = oldCurrentPalette;
                        this->selectedPaletteRow = oldSelectedRow;
                    }
                ));
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            selectedPaletteRow = -1;
        }
    }
}

void Sofanthiel::handlePaletteContextMenu()
{
    static ImVec2 rightClickStart;
    static bool rightClickTracking = false;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        rightClickStart = ImGui::GetIO().MousePos;
        rightClickTracking = true;
    }

    if (rightClickTracking && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        ImVec2 delta(
            ImGui::GetIO().MousePos.x - rightClickStart.x,
            ImGui::GetIO().MousePos.y - rightClickStart.y
        );
        float dragDist = sqrtf(delta.x * delta.x + delta.y * delta.y);
        rightClickTracking = false;

        if (dragDist < 5.0f) {
            ImGui::OpenPopup("PaletteContextMenu");
        }
    }

    if (ImGui::BeginPopup("PaletteContextMenu")) {
        bool hasRow = selectedPaletteRow >= 0 && selectedPaletteRow < static_cast<int>(palettes.size());
        bool canAdd = palettes.size() < 16;

        if (ImGui::MenuItem(ICON_FA_COPY " Copy", InputManager::Copy.label.c_str(), false, hasRow)) {
            paletteClipboard = palettes[selectedPaletteRow];
            hasPaletteClipboard = true;
        }

        if (ImGui::MenuItem(ICON_FA_PASTE " Paste", InputManager::Paste.label.c_str(), false, hasRow && hasPaletteClipboard)) {
            std::vector<Palette> oldPalettes = palettes;
            palettes[selectedPaletteRow] = paletteClipboard;
            std::vector<Palette> newPalettes = palettes;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Paste Palette Row",
                [this, newPalettes]() { this->palettes = newPalettes; },
                [this, oldPalettes]() { this->palettes = oldPalettes; }
            ));
        }

        ImGui::Separator();

        if (ImGui::MenuItem(ICON_FA_CLONE " Duplicate", nullptr, false, hasRow && canAdd)) {
            std::vector<Palette> oldPalettes = palettes;
            int oldCurrentPalette = currentPalette;
            int oldSelectedRow = selectedPaletteRow;

            Palette dup = palettes[selectedPaletteRow];
            palettes.insert(palettes.begin() + selectedPaletteRow + 1, dup);
            selectedPaletteRow = selectedPaletteRow + 1;

            std::vector<Palette> newPalettes = palettes;
            int newSelectedRow = selectedPaletteRow;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Duplicate Palette Row",
                [this, newPalettes, newSelectedRow]() {
                    this->palettes = newPalettes;
                    this->selectedPaletteRow = newSelectedRow;
                },
                [this, oldPalettes, oldCurrentPalette, oldSelectedRow]() {
                    this->palettes = oldPalettes;
                    this->currentPalette = oldCurrentPalette;
                    this->selectedPaletteRow = oldSelectedRow;
                }
            ));
        }

        bool canMoveUp = hasRow && selectedPaletteRow > 0;
        bool canMoveDown = hasRow && selectedPaletteRow < static_cast<int>(palettes.size()) - 1;

        if (ImGui::MenuItem(ICON_FA_ARROW_UP " Move Up", nullptr, false, canMoveUp)) {
            std::vector<Palette> oldPalettes = palettes;
            int oldSelectedRow = selectedPaletteRow;

            std::swap(palettes[selectedPaletteRow], palettes[selectedPaletteRow - 1]);
            selectedPaletteRow--;

            std::vector<Palette> newPalettes = palettes;
            int newSelectedRow = selectedPaletteRow;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Move Palette Up",
                [this, newPalettes, newSelectedRow]() {
                    this->palettes = newPalettes;
                    this->selectedPaletteRow = newSelectedRow;
                },
                [this, oldPalettes, oldSelectedRow]() {
                    this->palettes = oldPalettes;
                    this->selectedPaletteRow = oldSelectedRow;
                }
            ));
        }

        if (ImGui::MenuItem(ICON_FA_ARROW_DOWN " Move Down", nullptr, false, canMoveDown)) {
            std::vector<Palette> oldPalettes = palettes;
            int oldSelectedRow = selectedPaletteRow;

            std::swap(palettes[selectedPaletteRow], palettes[selectedPaletteRow + 1]);
            selectedPaletteRow++;

            std::vector<Palette> newPalettes = palettes;
            int newSelectedRow = selectedPaletteRow;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Move Palette Down",
                [this, newPalettes, newSelectedRow]() {
                    this->palettes = newPalettes;
                    this->selectedPaletteRow = newSelectedRow;
                },
                [this, oldPalettes, oldSelectedRow]() {
                    this->palettes = oldPalettes;
                    this->selectedPaletteRow = oldSelectedRow;
                }
            ));
        }

        ImGui::Separator();

        if (ImGui::MenuItem(ICON_FA_ERASER " Delete", InputManager::Delete.label.c_str(), false, hasRow && palettes.size() > 1)) {
            std::vector<Palette> oldPalettes = palettes;
            int oldSelectedRow = selectedPaletteRow;
            int oldCurrentPalette = currentPalette;

            palettes.erase(palettes.begin() + selectedPaletteRow);
            currentPalette = SDL_clamp(currentPalette, 0, static_cast<int>(palettes.size()) - 1);
            if (selectedPaletteRow >= static_cast<int>(palettes.size())) {
                selectedPaletteRow = static_cast<int>(palettes.size()) - 1;
            }

            std::vector<Palette> newPalettes = palettes;
            int newCurrentPalette = currentPalette;
            int newSelectedRow = selectedPaletteRow;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Delete Palette Row",
                [this, newPalettes, newCurrentPalette, newSelectedRow]() {
                    this->palettes = newPalettes;
                    this->currentPalette = newCurrentPalette;
                    this->selectedPaletteRow = newSelectedRow;
                },
                [this, oldPalettes, oldCurrentPalette, oldSelectedRow]() {
                    this->palettes = oldPalettes;
                    this->currentPalette = oldCurrentPalette;
                    this->selectedPaletteRow = oldSelectedRow;
                }
            ));
        }

        ImGui::Separator();

        if (ImGui::MenuItem(ICON_FA_XMARK " Clear Selection", "Esc", false, hasRow)) {
            selectedPaletteRow = -1;
        }

        ImGui::EndPopup();
    }
}

void Sofanthiel::drawPaletteInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin)
{
    ImGui::Text("Zoom: %.0f%%", view.zoom * 100);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("Pal: %.0f  Color: %.0f",
        SDL_clamp(floor((mousePosInWindow.y / view.zoom / 16)), 0, palettes.empty() ? 0 : palettes.size() - 1),
        SDL_clamp(floor((mousePosInWindow.x / view.zoom / 16)), 0, 15));

    float resetWidth = ImGui::CalcTextSize(ICON_FA_ROTATE " Reset").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - resetWidth - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button(ICON_FA_ROTATE " Reset")) {
        view.resetView();
    }
}
