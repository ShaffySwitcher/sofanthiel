#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"
#include "UndoRedo.h"

void Sofanthiel::handleCelInfobar() {
    ImGui::Begin("Cel Info", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    if (ImGui::Button(ICON_FA_ARROW_LEFT " Back")) {
        this->celEditingMode = false;
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (editingCelIndex >= 0 && editingCelIndex < static_cast<int>(animationCels.size())) {
        ImGui::Text(ICON_FA_LAYER_GROUP " %s", animationCels[editingCelIndex].name.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "No Cel Selected");
    }

    float checkWidth1 = ImGui::CalcTextSize("Border").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetFrameHeight();
    float checkWidth2 = ImGui::CalcTextSize("Emphasize").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetFrameHeight();
    float totalRight = checkWidth1 + checkWidth2 + ImGui::GetStyle().ItemSpacing.x * 3 + ImGui::GetFrameHeight();

    ImGui::SameLine(ImGui::GetWindowWidth() - totalRight - ImGui::GetStyle().WindowPadding.x);
    ImGui::Checkbox("Border", &showSelectionBorder);
    ImGui::SameLine();
    ImGui::Checkbox("Emphasize", &emphasizeSelectedOAMs);

    ImGui::End();
}

void Sofanthiel::handleCelPreview() {
    ImGui::Begin("Cel Preview", nullptr, ImGuiWindowFlags_NoCollapse);

    if (editingCelIndex < 0 || editingCelIndex >= static_cast<int>(animationCels.size())) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), ICON_FA_LAYER_GROUP " No cel selected for editing");
        ImGui::End();
        return;
    }

    float infoBarHeight = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y;
    float contentHeight = ImMax(ImGui::GetContentRegionAvail().y - infoBarHeight, 50.0f);

    ImGui::BeginChild("CelPreviewContent", ImVec2(0, contentHeight), ImGuiChildFlags_None);

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 baseSize = ImVec2(256, 256);
    ImVec2 origin = previewView.calculateOrigin(contentCenter, baseSize);

    drawCelPreviewContent(origin);

    if (!selectedOAMIndices.empty() && editingCelIndex >= 0 &&
        editingCelIndex < animationCels.size() && !isOAMDragging) {

        AnimationCel& cel = animationCels[editingCelIndex];
        bool moved = false;
        int deltaX = 0, deltaY = 0;

        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            deltaX = -1;
            moved = true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            deltaX = 1;
            moved = true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            deltaY = -1;
            moved = true;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            deltaY = 1;
            moved = true;
        }

        if (moved) {
            std::vector<std::pair<int, ImVec2>> beforePositions;
            for (int idx : selectedOAMIndices) {
                if (idx >= 0 && idx < cel.oams.size()) {
                    beforePositions.push_back({ idx, ImVec2((float)cel.oams[idx].xPosition, (float)cel.oams[idx].yPosition) });
                }
            }

            for (int idx : selectedOAMIndices) {
                if (idx >= 0 && idx < cel.oams.size()) {
                    cel.oams[idx].xPosition = SDL_clamp(
                        cel.oams[idx].xPosition + deltaX,
                        -128, 127
                    );
                    cel.oams[idx].yPosition = SDL_clamp(
                        cel.oams[idx].yPosition + deltaY,
                        -128, 127
                    );
                }
            }

            std::vector<std::pair<int, ImVec2>> afterPositions;
            for (int idx : selectedOAMIndices) {
                if (idx >= 0 && idx < cel.oams.size()) {
                    afterPositions.push_back({ idx, ImVec2((float)cel.oams[idx].xPosition, (float)cel.oams[idx].yPosition) });
                }
            }

            int celIdx = editingCelIndex;
            undoManager.execute(std::make_unique<LambdaAction>(
                "Move OAM(s)",
                [this, celIdx, afterPositions]() {
                    if (celIdx >= 0 && celIdx < animationCels.size()) {
                        for (size_t i = 0; i < afterPositions.size(); ++i) {
                            int oamIdx = afterPositions[i].first;
                            ImVec2 pos = afterPositions[i].second;
                            if (oamIdx >= 0 && oamIdx < (int)animationCels[celIdx].oams.size()) {
                                animationCels[celIdx].oams[oamIdx].xPosition = (int)pos.x;
                                animationCels[celIdx].oams[oamIdx].yPosition = (int)pos.y;
                            }
                        }
                    }
                },
                [this, celIdx, beforePositions]() {
                    if (celIdx >= 0 && celIdx < animationCels.size()) {
                        for (size_t i = 0; i < beforePositions.size(); ++i) {
                            int oamIdx = beforePositions[i].first;
                            ImVec2 pos = beforePositions[i].second;
                            if (oamIdx >= 0 && oamIdx < (int)animationCels[celIdx].oams.size()) {
                                animationCels[celIdx].oams[oamIdx].xPosition = (int)pos.x;
                                animationCels[celIdx].oams[oamIdx].yPosition = (int)pos.y;
                            }
                        }
                    }
                }
            ));
        }
    }

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("CelPreviewInfo", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImVec2 mousePosInWindow = ImVec2(
        ImGui::GetIO().MousePos.x - origin.x,
        ImGui::GetIO().MousePos.y - origin.y
    );

    drawCelPreviewInfoPanel(previewView, mousePosInWindow, baseSize, origin);

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::drawCelPreviewContent(const ImVec2& origin) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const AnimationCel& cel = animationCels[editingCelIndex];

    ImVec2 baseSize = ImVec2(256, 256);
    ImVec2 backgroundSize = ImVec2(GBA_WIDTH, GBA_HEIGHT);
    ImVec2 scaledSize = previewView.getScaledSize(baseSize);
    ImVec2 scaledBGSize = previewView.getScaledSize(baseSize);
    ImVec2 contentCenter = calculateContentCenter();

    ImVec2 originBG = previewView.calculateOrigin(contentCenter, backgroundSize);
    originBG.x += backgroundOffset.x * previewView.zoom;
    originBG.y += backgroundOffset.y * previewView.zoom;

    if (showBackgroundTexture && backgroundTexture != nullptr) {
        drawBackgroundTexture(drawList, originBG, scaledBGSize);
    }

    drawBackground(drawList, origin, scaledSize, previewView.backgroundColor);

    drawList->AddRect(
        origin,
        ImVec2(origin.x + scaledSize.x, origin.y + scaledSize.y),
        IM_COL32(100, 100, 100, 255)
    );
    float offsetX = baseSize.x / 2.0f;
    float offsetY = baseSize.y / 2.0f;

    float crosshairSize = getScaledSize(10.0f) * previewView.zoom;
    drawList->AddLine(
        ImVec2(origin.x + offsetX * previewView.zoom - crosshairSize, origin.y + offsetY * previewView.zoom),
        ImVec2(origin.x + offsetX * previewView.zoom + crosshairSize, origin.y + offsetY * previewView.zoom),
        IM_COL32(255, 0, 0, 255)
    );
    drawList->AddLine(
        ImVec2(origin.x + offsetX * previewView.zoom, origin.y + offsetY * previewView.zoom - crosshairSize),
        ImVec2(origin.x + offsetX * previewView.zoom, origin.y + offsetY * previewView.zoom + crosshairSize),
        IM_COL32(255, 0, 0, 255)
    );

    for (int i = cel.oams.size() - 1; i >= 0; i--) {
        const TengokuOAM& oam = cel.oams[i];
        bool isSelected = std::find(selectedOAMIndices.begin(), selectedOAMIndices.end(), i) != selectedOAMIndices.end();

        float alpha = (!isSelected && emphasizeSelectedOAMs) ? 0.7f : 1.0f;
        renderOAM(drawList, origin, previewView.zoom, oam, offsetX, offsetY, alpha);

        if (isSelected && showSelectionBorder) {
            int width = 0, height = 0;
            getOAMDimensions(oam.objShape, oam.objSize, width, height);
            float xPos = origin.x + (oam.xPosition + offsetX) * previewView.zoom;
            float yPos = origin.y + (oam.yPosition + offsetY) * previewView.zoom;

            drawList->AddRect(
                ImVec2(xPos, yPos),
                ImVec2(xPos + width * previewView.zoom, yPos + height * previewView.zoom),
                IM_COL32(255, 255, 0, 255),
                0.0f, 0, 2.0f
            );
        }
    }

    drawGrid(drawList, origin, scaledSize, previewView.zoom);

    handleOAMDragging(origin, offsetX, offsetY);

    previewView.handleZoomAndPan(ImGui::IsWindowHovered(), origin);
}

void Sofanthiel::handleOAMDragging(const ImVec2& origin, float offsetX, float offsetY) {
    if (editingCelIndex < 0 || editingCelIndex >= animationCels.size() || selectedOAMIndices.empty()) {
        return;
    }

    AnimationCel& cel = animationCels[editingCelIndex];
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isOverSelectedOAM = false;

    for (int idx : selectedOAMIndices) {
        if (idx >= 0 && idx < cel.oams.size()) {
            TengokuOAM& oam = cel.oams[idx];
            int width = 0, height = 0;
            getOAMDimensions(oam.objShape, oam.objSize, width, height);

            float xPos = origin.x + (oam.xPosition + offsetX) * previewView.zoom;
            float yPos = origin.y + (oam.yPosition + offsetY) * previewView.zoom;

            if (mousePos.x >= xPos && mousePos.x < xPos + width * previewView.zoom &&
                mousePos.y >= yPos && mousePos.y < yPos + height * previewView.zoom) {
                isOverSelectedOAM = true;
                break;
            }
        }
    }

    if (isOverSelectedOAM && !isOAMDragging) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (ImGui::IsWindowHovered() || isOAMDragging) {
        if (!isOAMDragging && isOverSelectedOAM && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isOAMDragging = true;
            oamDragStart = mousePos;

            selectedOAMStartPositions.clear();
            for (int idx : selectedOAMIndices) {
                if (idx >= 0 && idx < cel.oams.size()) {
                    selectedOAMStartPositions.push_back(ImVec2(
                        cel.oams[idx].xPosition,
                        cel.oams[idx].yPosition
                    ));
                }
            }
        }

        if (isOAMDragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImVec2(
                    mousePos.x - oamDragStart.x,
                    mousePos.y - oamDragStart.y
                );

                int deltaX = static_cast<int>(delta.x / previewView.zoom);
                int deltaY = static_cast<int>(delta.y / previewView.zoom);

                for (size_t i = 0; i < selectedOAMIndices.size(); i++) {
                    int idx = selectedOAMIndices[i];
                    if (idx >= 0 && idx < cel.oams.size() && i < selectedOAMStartPositions.size()) {
                        cel.oams[idx].xPosition = SDL_clamp(
                            selectedOAMStartPositions[i].x + deltaX,
                            -128, 127
                        );
                        cel.oams[idx].yPosition = SDL_clamp(
                            selectedOAMStartPositions[i].y + deltaY,
                            -128, 127
                        );
                    }
                }

                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            else {
                int celIdx = editingCelIndex;
                std::vector<std::pair<int, ImVec2>> beforePositions;
                std::vector<std::pair<int, ImVec2>> afterPositions;

                for (size_t i = 0; i < selectedOAMIndices.size(); i++) {
                    int idx = selectedOAMIndices[i];
                    if (idx >= 0 && idx < cel.oams.size() && i < selectedOAMStartPositions.size()) {
                        beforePositions.push_back({ idx, selectedOAMStartPositions[i] });
                        afterPositions.push_back({ idx, ImVec2((float)cel.oams[idx].xPosition, (float)cel.oams[idx].yPosition) });
                    }
                }

                bool changed = false;
                for (size_t i = 0; i < beforePositions.size(); i++) {
                    if (beforePositions[i].second.x != afterPositions[i].second.x ||
                        beforePositions[i].second.y != afterPositions[i].second.y) {
                        changed = true;
                        break;
                    }
                }

                if (changed) {
                    undoManager.execute(std::make_unique<LambdaAction>(
                        "Drag OAM(s)",
                        [this, celIdx, afterPositions]() {
                            if (celIdx >= 0 && celIdx < animationCels.size()) {
                                for (size_t i = 0; i < afterPositions.size(); ++i) {
                                    int oamIdx = afterPositions[i].first;
                                    ImVec2 pos = afterPositions[i].second;
                                    if (oamIdx >= 0 && oamIdx < (int)animationCels[celIdx].oams.size()) {
                                        animationCels[celIdx].oams[oamIdx].xPosition = (int)pos.x;
                                        animationCels[celIdx].oams[oamIdx].yPosition = (int)pos.y;
                                    }
                                }
                            }
                        },
                        [this, celIdx, beforePositions]() {
                            if (celIdx >= 0 && celIdx < animationCels.size()) {
                                for (size_t i = 0; i < beforePositions.size(); ++i) {
                                    int oamIdx = beforePositions[i].first;
                                    ImVec2 pos = beforePositions[i].second;
                                    if (oamIdx >= 0 && oamIdx < (int)animationCels[celIdx].oams.size()) {
                                        animationCels[celIdx].oams[oamIdx].xPosition = (int)pos.x;
                                        animationCels[celIdx].oams[oamIdx].yPosition = (int)pos.y;
                                    }
                                }
                            }
                        }
                    ));
                }

                isOAMDragging = false;
                selectedOAMStartPositions.clear();
            }
        }
    }
    else if (isOAMDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        isOAMDragging = false;
        selectedOAMStartPositions.clear();
    }
}

void Sofanthiel::drawCelPreviewInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin) {
    ImGui::ColorEdit4("##CelBG", view.backgroundColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Background Color");

    ImGui::SameLine();
    if (ImGui::Checkbox("BG Image", &showBackgroundTexture)) {
        this->previewView.backgroundColor[3] = !showBackgroundTexture ? 1.0f : 0.5f;
    }

    if (showBackgroundTexture && backgroundTexture != nullptr) {
        ImGui::SameLine();
        ImGui::Text("Offset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(getScaledSize(80));
        ImGui::DragFloat2("##BGOffset", (float*)&backgroundOffset, 1.0f, -500.0f, 500.0f, "%.0f");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid);

    float resetWidth = ImGui::CalcTextSize(ICON_FA_ROTATE " Reset").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - resetWidth - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button(ICON_FA_ROTATE " Reset")) {
        view.resetView();
    }

    ImGui::Text("Zoom: %.0f%%  Cursor: (%.0f, %.0f)",
        view.zoom * 100,
        SDL_clamp(floor(mousePosInWindow.x / view.zoom) - 128, -128, 127),
        SDL_clamp(floor(mousePosInWindow.y / view.zoom) - 128, -128, 127));
}

void Sofanthiel::handleCelOAMs() {
    ImGui::Begin("OAMs", nullptr, ImGuiWindowFlags_NoCollapse);

    if (editingCelIndex < 0 || editingCelIndex >= static_cast<int>(animationCels.size())) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), ICON_FA_LIST " No cel selected for editing");
        ImGui::End();
        return;
    }

    AnimationCel& cel = animationCels[editingCelIndex];

    if (ImGui::Button(ICON_FA_PLUS " Add OAM")) {
        TengokuOAM newOAM;
        newOAM.xPosition = 0;
        newOAM.yPosition = 0;
        newOAM.tileID = 0;
        newOAM.palette = 0;
        newOAM.objShape = SHAPE_SQUARE;
        newOAM.objSize = 0;
        newOAM.hFlip = false;
        newOAM.vFlip = false;

        int celIdx = editingCelIndex;
        auto oamsBefore = cel.oams;
        cel.oams.push_back(newOAM);
        auto oamsAfter = cel.oams;

        selectedOAMIndices.clear();
        selectedOAMIndices.push_back(cel.oams.size() - 1);

        undoManager.execute(std::make_unique<OAMModifyAction>(
            "Add OAM",
            [this, celIdx]() -> std::vector<TengokuOAM>* {
                if (celIdx >= 0 && celIdx < static_cast<int>(animationCels.size())) {
                    return &animationCels[celIdx].oams;
                }
                return nullptr;
            },
            oamsBefore,
            oamsAfter
        ));
    }

    ImGui::Separator();

    ImGui::BeginChild("OAMsList", ImVec2(0, 0), ImGuiChildFlags_Borders);

    bool itemInteracted = false;

    for (int i = 0; i < cel.oams.size(); i++) {
        TengokuOAM& oam = cel.oams[i];

        ImGui::PushID(i);

        char label[128];
        int width, height;
        getOAMDimensions(oam.objShape, oam.objSize, width, height);
        snprintf(label, sizeof(label), "OAM %d: Tile %d, %dx%d at (%d,%d)",
            i, oam.tileID, width, height, oam.xPosition, oam.yPosition);

        bool isSelected = std::find(selectedOAMIndices.begin(), selectedOAMIndices.end(), i) != selectedOAMIndices.end();

        if (i == oamDragHoverItem) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.5f, 0.7f, 0.5f));
        }

        if (ImGui::Selectable(label, isSelected)) {
            itemInteracted = true;

            if (ImGui::GetIO().KeyCtrl) {
                if (isSelected) {
                    selectedOAMIndices.erase(
                        std::remove(selectedOAMIndices.begin(), selectedOAMIndices.end(), i),
                        selectedOAMIndices.end());
                }
                else {
                    selectedOAMIndices.push_back(i);
                }
            }
            else if (ImGui::GetIO().KeyShift && !selectedOAMIndices.empty()) {
                int lastSelected = selectedOAMIndices.back();
                selectedOAMIndices.clear();

                int start = std::min(lastSelected, i);
                int end = std::max(lastSelected, i);
                for (int idx = start; idx <= end; idx++) {
                    selectedOAMIndices.push_back(idx);
                }
            }
            else {
                selectedOAMIndices.clear();
                selectedOAMIndices.push_back(i);
            }
        }

        if (ImGui::BeginPopupContextItem()) {
            itemInteracted = true;
            bool hasSelection = !selectedOAMIndices.empty();

            if (!isSelected && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                selectedOAMIndices.clear();
                selectedOAMIndices.push_back(i);
                isSelected = true;
            }

            if (ImGui::MenuItem("Remove", nullptr, false, hasSelection)) {
                int celIdx = editingCelIndex;
                auto oamsBefore = cel.oams;

                std::sort(selectedOAMIndices.begin(), selectedOAMIndices.end(), std::greater<int>());

                for (int index : selectedOAMIndices) {
                    if (index >= 0 && index < cel.oams.size()) {
                        cel.oams.erase(cel.oams.begin() + index);
                    }
                }

                auto oamsAfter = cel.oams;
                selectedOAMIndices.clear();

                undoManager.execute(std::make_unique<OAMModifyAction>(
                    "Remove OAM",
                    [this, celIdx]() -> std::vector<TengokuOAM>* {
                        if (celIdx >= 0 && celIdx < static_cast<int>(animationCels.size())) {
                            return &animationCels[celIdx].oams;
                        }
                        return nullptr;
                    },
                    oamsBefore,
                    oamsAfter
                ));
            }

            ImGui::EndPopup();
        }

        if (i == oamDragHoverItem) {
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            itemInteracted = true;
            ImGui::SetDragDropPayload("DND_OAM_INDEX", &i, sizeof(int));
            ImGui::Text("Moving OAM %d", i);
            oamDraggedItem = i;
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            itemInteracted = true;
            oamDragHoverItem = i;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_OAM_INDEX")) {
                int srcIdx = *(const int*)payload->Data;

                if (srcIdx != i) {
                    TengokuOAM temp = cel.oams[srcIdx];

                    if (srcIdx < i) {
                        cel.oams.erase(cel.oams.begin() + srcIdx);
                        cel.oams.insert(cel.oams.begin() + (i - 1), temp);

                        for (size_t s = 0; s < selectedOAMIndices.size(); s++) {
                            int& selIdx = selectedOAMIndices[s];
                            if (selIdx == srcIdx)
                                selIdx = i - 1;
                            else if (selIdx > srcIdx && selIdx <= i - 1)
                                selIdx--;
                        }
                    }
                    else {
                        cel.oams.erase(cel.oams.begin() + srcIdx);
                        cel.oams.insert(cel.oams.begin() + i, temp);

                        for (size_t s = 0; s < selectedOAMIndices.size(); s++) {
                            int& selIdx = selectedOAMIndices[s];
                            if (selIdx == srcIdx)
                                selIdx = i;
                            else if (selIdx < srcIdx && selIdx >= i)
                                selIdx++;
                        }
                    }
                }

                oamDraggedItem = -1;
                oamDragHoverItem = -1;
            }

            ImGui::EndDragDropTarget();
        }
        else if (oamDraggedItem >= 0 && i != oamDraggedItem) {
            if (ImGui::IsMouseHoveringRect(
                ImGui::GetItemRectMin(),
                ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + 3))) {
                oamDragHoverItem = i;
                itemInteracted = true;
            }
        }

        if (isSelected) {
            ImGui::SetItemDefaultFocus();
        }

        ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::BeginDragDropTarget()) {
        itemInteracted = true;
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_OAM_INDEX")) {
            int srcIdx = *(const int*)payload->Data;

            TengokuOAM temp = cel.oams[srcIdx];
            cel.oams.erase(cel.oams.begin() + srcIdx);
            cel.oams.push_back(temp);

            for (size_t s = 0; s < selectedOAMIndices.size(); s++) {
                int& selIdx = selectedOAMIndices[s];
                if (selIdx == srcIdx)
                    selIdx = cel.oams.size() - 1;
                else if (selIdx > srcIdx)
                    selIdx--;
            }

            oamDraggedItem = -1;
            oamDragHoverItem = -1;
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !itemInteracted) {
        selectedOAMIndices.clear();
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        oamDraggedItem = -1;
        oamDragHoverItem = -1;
    }

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::handleCelEditor() {
    ImGui::Begin("Cel Editor", nullptr, ImGuiWindowFlags_NoCollapse);

    if (editingCelIndex < 0 || editingCelIndex >= static_cast<int>(animationCels.size())) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), ICON_FA_SLIDERS " No cel selected for editing");
        ImGui::End();
        return;
    }

    AnimationCel& cel = animationCels[editingCelIndex];

    if (selectedOAMIndices.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), ICON_FA_HAND_POINTER " Select an OAM to edit its properties");
        ImGui::End();
        return;
    }

    int editIdx = selectedOAMIndices[0];
    if (editIdx < 0 || editIdx >= cel.oams.size()) {
        ImGui::End();
        return;
    }

    TengokuOAM& oam = cel.oams[editIdx];
    int xPosition = oam.xPosition;
    int yPosition = oam.yPosition;
    int tileID = oam.tileID;
    int palette = oam.palette;
    int objShape = oam.objShape;
    int objSize = oam.objSize;
    bool hFlip = oam.hFlip;
    bool vFlip = oam.vFlip;

    float availWidth = ImGui::GetContentRegionAvail().x;
    float halfWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (selectedOAMIndices.size() > 1) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f),
            "Editing %zu OAMs (showing OAM %d properties)",
            selectedOAMIndices.size(), editIdx);
    }
    else {
        ImGui::Text("Editing OAM %d in cel '%s'", editIdx, cel.name.c_str());
    }
    ImGui::Separator();

    ImGui::Text("Position:");
    ImGui::Columns(2, "PositionColumns", false);
    ImGui::SetColumnWidth(0, halfWidth);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
    if (ImGui::DragInt("X##pos", &xPosition, 1.0f, -128, 127, "%d")) {
        int delta = xPosition - oam.xPosition;
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].xPosition = SDL_clamp(cel.oams[idx].xPosition + delta, -128, 127);
            }
        }
    }
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
    if (ImGui::DragInt("Y##pos", &yPosition, 1.0f, -128, 127, "%d")) {
        int delta = yPosition - oam.yPosition;
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].yPosition = SDL_clamp(cel.oams[idx].yPosition + delta, -128, 127);
            }
        }
    }
    ImGui::Columns(1);

    ImGui::Spacing();

    ImGui::Columns(2, "TilePaletteColumns", false);
    ImGui::SetColumnWidth(0, halfWidth);

    ImGui::Text("Tile ID:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
    if (ImGui::InputInt("##tileid", &tileID, 1, 10)) {
        int clampedTileID = SDL_clamp(tileID, 0, tiles.getSize() - 1);
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].tileID = clampedTileID;
            }
        }
    }
    ImGui::NextColumn();

    ImGui::Text("Palette:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
    if (ImGui::SliderInt("##palette", &palette, 0, palettes.size() - 1)) {
        int clampedPalette = SDL_clamp(palette, 0, palettes.size() - 1);
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].palette = clampedPalette;
            }
        }
    }
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* shapes[] = { "Square", "Horizontal", "Vertical" };
    ImGui::Text("Shape and Size:");
    ImGui::Columns(2, "ShapeSizeColumns", false);
    ImGui::SetColumnWidth(0, halfWidth);

    ImGui::Text("Shape:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
    if (ImGui::Combo("##shape", &objShape, shapes, IM_ARRAYSIZE(shapes))) {
        if (objShape == SHAPE_SQUARE && objSize > 3) objSize = 3;
        else if (objShape == SHAPE_HORIZONTAL && objSize > 3) objSize = 3;
        else if (objShape == SHAPE_VERTICAL && objSize > 3) objSize = 3;

        int clampedShape = SDL_clamp(objShape, 0, 2);
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].objShape = clampedShape;

                if (clampedShape != oam.objShape) {
                    int validSize = cel.oams[idx].objSize;
                    if (clampedShape == SHAPE_SQUARE && validSize > 3) validSize = 3;
                    else if (clampedShape == SHAPE_HORIZONTAL && validSize > 3) validSize = 3;
                    else if (clampedShape == SHAPE_VERTICAL && validSize > 3) validSize = 3;
                    cel.oams[idx].objSize = validSize;
                }
            }
        }
    }
    ImGui::NextColumn();

    ImGui::Text("Size:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);

    bool sizeChanged = false;
    if (objShape == SHAPE_SQUARE) {
        const char* squareSizes[] = { "8x8", "16x16", "32x32", "64x64" };
        sizeChanged = ImGui::Combo("##size", &objSize, squareSizes, IM_ARRAYSIZE(squareSizes));
    }
    else if (objShape == SHAPE_HORIZONTAL) {
        const char* horizontalSizes[] = { "16x8", "32x8", "32x16", "64x32" };
        sizeChanged = ImGui::Combo("##size", &objSize, horizontalSizes, IM_ARRAYSIZE(horizontalSizes));
    }
    else if (objShape == SHAPE_VERTICAL) {
        const char* verticalSizes[] = { "8x16", "8x32", "16x32", "32x64" };
        sizeChanged = ImGui::Combo("##size", &objSize, verticalSizes, IM_ARRAYSIZE(verticalSizes));
    }

    if (sizeChanged) {
        int clampedSize = SDL_clamp(objSize, 0, 3);
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].objSize = clampedSize;
            }
        }
    }

    ImGui::Columns(1);

    int width, height;
    getOAMDimensions(objShape, objSize, width, height);

    ImGui::Spacing();
    ImGui::Text("Dimensions: %dx%d pixels", width, height);
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Columns(2, "FlipColumns", false);
    ImGui::SetColumnWidth(0, halfWidth);

    if (ImGui::Checkbox("Horizontal Flip", &hFlip)) {
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].hFlip = hFlip;
            }
        }
    }
    ImGui::NextColumn();

    if (ImGui::Checkbox("Vertical Flip", &vFlip)) {
        for (int idx : selectedOAMIndices) {
            if (idx >= 0 && idx < cel.oams.size()) {
                cel.oams[idx].vFlip = vFlip;
            }
        }
    }
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Tiles Used: %d (%d x %d)", (width / 8) * (height / 8), width / 8, height / 8);
    ImGui::Text("Tile Range: %d to %d", tileID, tileID + (width / 8) * (height / 8) - 1);

    ImGui::End();
}

void Sofanthiel::handleCelSpritesheet()
{
    ImGui::Begin("Spritesheet##", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::BeginChild("CelSpritesheetContent", ImVec2(0, 0), ImGuiChildFlags_None);

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 baseSize = ImVec2(tiles.getWidth(), tiles.getHeight());
    ImVec2 origin = spritesheetView.calculateOrigin(contentCenter, baseSize);

    drawCelSpritesheetContent(origin);

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::drawCelSpritesheetContent(const ImVec2& origin)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 scaledSize = ImVec2(
        tiles.getWidth() * spritesheetView.zoom,
        tiles.getHeight() * spritesheetView.zoom
    );

    drawBackground(drawList, origin, scaledSize, spritesheetView.backgroundColor);
    drawCelSpritesheetTiles(drawList, origin);

    drawList->AddRect(
        origin,
        ImVec2(origin.x + scaledSize.x, origin.y + scaledSize.y),
        IM_COL32(100, 100, 100, 255)
    );

    drawGrid(drawList, origin, scaledSize, spritesheetView.zoom);

    spritesheetView.handleZoomAndPan(ImGui::IsWindowHovered(), origin);

    handleCelSpritesheetClicks(origin);
}

void Sofanthiel::drawCelSpritesheetTiles(ImDrawList* drawList, const ImVec2& origin)
{
    if (tiles.getSize() <= 0 || palettes.empty()) {
        ImVec2 textPos = ImGui::GetCursorScreenPos();
        textPos.x += ImGui::GetContentRegionAvail().x * 0.5f - ImGui::CalcTextSize("No tiles or palette loaded").x * 0.5f;
        textPos.y += ImGui::GetContentRegionAvail().y * 0.5f;

        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "No tiles or palette loaded");
        return;
    }

    std::vector<int> usedTileIndices;
    if (editingCelIndex >= 0 && editingCelIndex < animationCels.size() && !selectedOAMIndices.empty()) {
        const AnimationCel& cel = animationCels[editingCelIndex];

        for (int oamIdx : selectedOAMIndices) {
            if (oamIdx >= 0 && oamIdx < cel.oams.size()) {
                const TengokuOAM& oam = cel.oams[oamIdx];

                int width, height;
                getOAMDimensions(oam.objShape, oam.objSize, width, height);

                for (int ty = 0; ty < height / 8; ty++) {
                    for (int tx = 0; tx < width / 8; tx++) {
                        int tileIdx = oam.tileID + ty * 32 + tx;
                        usedTileIndices.push_back(tileIdx);
                    }
                }
            }
        }
    }

    for (int i = 0; i < tiles.getSize(); i++) {
        float tileSize = 8.0f * spritesheetView.zoom;

        int tileX = i % TILES_PER_LINE;
        int tileY = i / TILES_PER_LINE;

        float xPos = origin.x + tileX * tileSize;
        float yPos = origin.y + tileY * tileSize;

        bool isUsedTile = std::find(usedTileIndices.begin(), usedTileIndices.end(), i) != usedTileIndices.end();

        if (isUsedTile) {
            drawList->AddRect(
                ImVec2(xPos - 1, yPos - 1),
                ImVec2(xPos + tileSize + 1, yPos + tileSize + 1),
                IM_COL32(255, 0, 0, 255),
                0.0f, 0, 2.0f
            );
        }

        int oldCurrentPalette = currentPalette;
        if (editingCelIndex >= 0 && editingCelIndex < static_cast<int>(animationCels.size()) &&
            !selectedOAMIndices.empty() && selectedOAMIndices[0] >= 0 &&
            selectedOAMIndices[0] < static_cast<int>(animationCels[editingCelIndex].oams.size())) {
            currentPalette = animationCels[editingCelIndex].oams[selectedOAMIndices[0]].palette;
        }
        drawSingleTile(drawList, xPos, yPos, i);
		currentPalette = oldCurrentPalette;
    }
}

void Sofanthiel::handleCelSpritesheetClicks(const ImVec2& origin)
{
    if (editingCelIndex < 0 || editingCelIndex >= animationCels.size() || selectedOAMIndices.empty()) {
        return;
    }

    if (!ImGui::IsWindowHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return;
    }

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    float tileSize = 8.0f * spritesheetView.zoom;

    int tileX = static_cast<int>((mousePos.x - origin.x) / tileSize);
    int tileY = static_cast<int>((mousePos.y - origin.y) / tileSize);

    if (tileX < 0 || tileY < 0 || tileX >= TILES_PER_LINE ||
        tileY * TILES_PER_LINE + tileX >= tiles.getSize()) {
        return;
    }

    int tileIndex = tileY * TILES_PER_LINE + tileX;

    AnimationCel& cel = animationCels[editingCelIndex];
    for (int idx : selectedOAMIndices) {
        if (idx >= 0 && idx < cel.oams.size()) {
            cel.oams[idx].tileID = tileIndex;
        }
    }
}
