#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"

void Sofanthiel::handleTimeline()
{
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse);

    if (currentAnimation < 0 || currentAnimation >= static_cast<int>(animations.size())) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 textSize = ImGui::CalcTextSize("No animation selected");
        ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, avail.y * 0.5f - textSize.y));
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), ICON_FA_FILM " No animation selected");
        ImGui::SetCursorPosX((avail.x - ImGui::CalcTextSize("Create or select an animation to begin").x) * 0.5f);
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f), "Create or select an animation to begin");
        ImGui::End();
        return;
    }

    Animation& anim = animations[currentAnimation];

    recalculateTotalFrames();

    drawTimelineControls();
    ImGui::Separator();

    float frameWidth = getScaledSize(15.0f);
    float timelineStartX = getScaledSize(60.0f);
    float requiredWidth = timelineStartX + frameWidth * totalFrames;

    drawTimelineHeaders(timelineStartX, syncScroll, frameWidth);
    drawTimelineContent(anim, timelineStartX, syncScroll, frameWidth, requiredWidth);

    ImGui::End();
}

void Sofanthiel::drawTimelineControls()
{
    float buttonSize = ImGui::GetFrameHeight();

    if (ImGui::Button(ICON_FA_BACKWARD_FAST, ImVec2(buttonSize, buttonSize))) {
        currentFrame = 0;
    }
    ImGui::SameLine(0, 2);

    if (ImGui::Button(ICON_FA_BACKWARD_STEP, ImVec2(buttonSize, buttonSize))) {
        currentFrame = (currentFrame > 0) ? currentFrame - 1 : totalFrames > 0 ? totalFrames - 1 : 0;
    }
    ImGui::SameLine(0, 2);

    bool wasPlaying = isPlaying;
    if (wasPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.55f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.65f, 0.45f, 1.0f));
    }
    if (ImGui::Button(isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, ImVec2(buttonSize, buttonSize))) {
        isPlaying = !isPlaying;
    }
    if (wasPlaying) {
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine(0, 2);

    if (ImGui::Button(ICON_FA_FORWARD_STEP, ImVec2(buttonSize, buttonSize))) {
        if (totalFrames > 0) currentFrame = (currentFrame + 1) % totalFrames;
    }
    ImGui::SameLine(0, 2);

    if (ImGui::Button(ICON_FA_FORWARD_FAST, ImVec2(buttonSize, buttonSize))) {
        currentFrame = totalFrames > 0 ? totalFrames - 1 : 0;
    }
    ImGui::SameLine();

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Text("%d / %d", currentFrame, totalFrames == 0 ? 0 : totalFrames - 1);

    float sliderWidth = getScaledSize(100);
    float loopWidth = ImGui::CalcTextSize("Loop").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetFrameHeight();
    float rightWidth = sliderWidth + loopWidth + ImGui::GetStyle().ItemSpacing.x * 3 + ImGui::CalcTextSize("FPS").x + ImGui::GetStyle().ItemInnerSpacing.x;

    ImGui::SameLine(ImGui::GetWindowWidth() - rightWidth - ImGui::GetStyle().WindowPadding.x);
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderFloat("FPS", &frameRate, 1.0f, 120.0f, "%.0f");
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &loopAnimation);
}

void Sofanthiel::drawTimelineHeaders(float timelineStartX, float& syncScroll, float frameWidth)
{
    float headerHeight = getScaledSize(25.0f);
    
    ImGui::BeginChild("TrackHeader", ImVec2(timelineStartX, headerHeight), false,
        ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("Frames");
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("TimelineHeader", ImVec2(0, headerHeight), false,
        ImGuiWindowFlags_NoScrollbar);

    for (int i = 0; i < totalFrames; i++) {
        if (i % 5 == 0) {
            float xPos = i * frameWidth - syncScroll;
            ImGui::SetCursorPosX(xPos);
            ImGui::Text("%d", i);
            ImGui::SameLine();
        }
    }
    ImGui::EndChild();

    ImGui::BeginChild("TrackLabels", ImVec2(timelineStartX, 0), false,
        ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(5, 0));
    ImGui::Text("Cels");
    ImGui::EndChild();
    ImGui::SameLine();
}

void Sofanthiel::drawTimelineContent(Animation& anim, float timelineStartX, float& syncScroll,
    float frameWidth, float requiredWidth)
{
    float timelineEntryHeight = getScaledSize(25.0f);

    ImGui::BeginChild("TimelineContent", ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);

    ImGui::SetCursorPosX(requiredWidth - timelineStartX);
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::SetCursorPos(ImVec2(0, 0));

    handleTimelineScrolling(syncScroll);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();

    drawTimelineMarker(drawList, winPos, syncScroll, frameWidth);

    if (anim.entries.empty()) {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        contentSize.y = ImMax(contentSize.y, timelineEntryHeight);

        ImVec2 textSize = ImGui::CalcTextSize("Drag animation cels here");
        float textX = winPos.x + (ImGui::GetWindowWidth() - textSize.x) * 0.5f;
        float textY = winPos.y + (timelineEntryHeight - textSize.y) * 0.5f;

        drawList->AddText(
            ImVec2(textX, textY),
            IM_COL32(150, 150, 150, 150),
            "Drag animation cels here"
        );

        ImGui::InvisibleButton("##empty_timeline_dropzone", contentSize);

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ANIM_CELL")) {
                if (payload->DataSize == sizeof(int)) {
                    int celIdx = *(const int*)payload->Data;
                    if (celIdx >= 0 && celIdx < animationCels.size() && currentAnimation >= 0 && currentAnimation < animations.size()) {
                        AnimationEntry newEntry;
                        newEntry.celName = animationCels[celIdx].name;
                        newEntry.duration = 10;

                        anim.entries.push_back(newEntry);

                        totalFrames = newEntry.duration;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    else {
        timelineHoveredEntryIdx = -1;

        if (ImGui::IsWindowFocused()) {
            if (InputManager::isPressed(InputManager::Copy) && !timelineSelectedEntryIndices.empty()) {
                clipboardEntries.clear();
                for (int idx : timelineSelectedEntryIndices) {
                    if (idx >= 0 && idx < anim.entries.size()) {
                        clipboardEntries.push_back(anim.entries[idx]);
                    }
                }
            }

            if (InputManager::isPressed(InputManager::Paste) && !clipboardEntries.empty()) {
                int insertPos = timelineSelectedEntryIndices.empty() ? anim.entries.size() :
                    *std::max_element(timelineSelectedEntryIndices.begin(), timelineSelectedEntryIndices.end()) + 1;

                auto oldEntries = anim.entries;

                anim.entries.insert(anim.entries.begin() + insertPos,
                    clipboardEntries.begin(), clipboardEntries.end());

                timelineSelectedEntryIndices.clear();
                for (size_t i = 0; i < clipboardEntries.size(); i++) {
                    timelineSelectedEntryIndices.push_back(insertPos + i);
                }

                int animIdx = currentAnimation;
                undoManager.execute(std::make_unique<AnimationEntriesAction>(
                    "Paste Timeline Entries",
                    [this, animIdx]() -> std::vector<AnimationEntry>* {
                        if (animIdx >= 0 && animIdx < static_cast<int>(animations.size())) {
                            return &animations[animIdx].entries;
                        }
                        return nullptr;
                    },
                    oldEntries,
                    anim.entries));

                recalculateTotalFrames();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                timelineSelectedEntryIndices.clear();
            }
        }

        if (!timelineDragState.isDragging) {
            drawTimelineEntries(anim, drawList, winPos, syncScroll, frameWidth, timelineResizeState, timelineSelectedEntryIndices, timelineEntryHeight, clipboardEntries);
        }
        else {
            int frameIndex = 0;
            for (int entryIdx = 0; entryIdx < anim.entries.size(); entryIdx++) {
                if (entryIdx == timelineDragState.draggedEntryIdx) {
                    frameIndex += anim.entries[entryIdx].duration;
                    continue;
                }

                AnimationEntry& entry = anim.entries[entryIdx];

                float celStartX = frameIndex * frameWidth;
                float celWidth = entry.duration * frameWidth;

                if (celStartX - syncScroll + celWidth < 0 || celStartX - syncScroll > ImGui::GetWindowWidth()) {
                    frameIndex += entry.duration;
                    continue;
                }

                bool isSelected = std::find(timelineSelectedEntryIndices.begin(), timelineSelectedEntryIndices.end(), entryIdx) != timelineSelectedEntryIndices.end();

                drawTimelineEntryBackground(drawList, winPos, syncScroll, entryIdx, celStartX, celWidth, isSelected, timelineSelectedEntryIndices, timelineHoveredEntryIdx, clipboardEntries, timelineEntryHeight);
                drawTimelineEntryLabel(drawList, winPos, syncScroll, entry.celName, celStartX, celWidth, timelineEntryHeight);
                drawTimelineFrameCels(drawList, winPos, syncScroll, frameIndex, entry.duration, frameWidth, timelineEntryHeight);

                frameIndex += entry.duration;
            }
        }

        handleTimelineResizing(anim, timelineResizeState, syncScroll, frameWidth);
        if (!timelineResizeState.isResizing) { handleTimelineDragging(anim, drawList, winPos, syncScroll, frameWidth, timelineDragState, timelineEntryHeight); }

        handleTimelineAutoScroll(syncScroll, frameWidth);

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && !timelineSelectedEntryIndices.empty()) {
            auto oldEntries = anim.entries;

            std::sort(timelineSelectedEntryIndices.begin(), timelineSelectedEntryIndices.end(), std::greater<int>());

            for (int idx : timelineSelectedEntryIndices) {
                if (idx >= 0 && idx < anim.entries.size()) {
                    anim.entries.erase(anim.entries.begin() + idx);
                }
            }

            undoManager.execute(std::make_unique<LambdaAction>(
                "Delete Timeline Entries",
                []{},
                [this, oldEntries, animIdx = currentAnimation](){
                    if (animIdx >= 0 && animIdx < animations.size())
                        animations[animIdx].entries = oldEntries;
                    recalculateTotalFrames();
                }
            ));

            timelineSelectedEntryIndices.clear();

            recalculateTotalFrames();
        }
    }


    if (!anim.entries.empty() && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ANIM_CELL")) {
            if (payload->DataSize == sizeof(int)) {
                int celIdx = *(const int*)payload->Data;
                if (celIdx >= 0 && celIdx < animationCels.size() && currentAnimation >= 0 && currentAnimation < animations.size()) {
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    float framePos = (mousePos.x - winPos.x + syncScroll) / frameWidth;
                    int frameIdx = static_cast<int>(framePos);

                    int insertPos = 0;
                    int frameCount = 0;
                    for (insertPos = 0; insertPos < anim.entries.size(); insertPos++) {
                        if (frameCount + anim.entries[insertPos].duration > frameIdx)
                            break;
                        frameCount += anim.entries[insertPos].duration;
                    }

                    AnimationEntry newEntry;
                    newEntry.celName = animationCels[celIdx].name;
                    newEntry.duration = 10;

                    if (insertPos < anim.entries.size()) {
                        insertPos++;
                    }
                    anim.entries.insert(anim.entries.begin() + insertPos, newEntry);

                    recalculateTotalFrames();
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
}

void Sofanthiel::handleTimelineScrolling(float& syncScroll)
{
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0 && !io.KeyShift && !io.KeyCtrl) {
        ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseWheel * getScaledSize(30.0f));
        io.MouseWheel = 0;
    }

    syncScroll = ImGui::GetScrollX();
}

void Sofanthiel::drawTimelineMarker(ImDrawList* drawList, const ImVec2& winPos,
    float syncScroll, float frameWidth)
{
    float currentFramePos = currentFrame * frameWidth;

    ImVec2 lineStart(winPos.x + currentFramePos - syncScroll, winPos.y);
    ImVec2 lineEnd(winPos.x + currentFramePos - syncScroll, winPos.y + ImGui::GetWindowHeight());
    drawList->AddLine(lineStart, lineEnd, IM_COL32(255, 128, 0, 255), 2.0f);
}

void Sofanthiel::drawTimelineEntries(Animation& anim, ImDrawList* drawList, const ImVec2& winPos,
    float syncScroll, float frameWidth, TimelineResizeState& resizeState, std::vector<int>& selectedEntryIndices, float entryHeight, std::vector<AnimationEntry>& clipboardEntries)
{
    int frameIndex = 0;

    for (int entryIdx = 0; entryIdx < anim.entries.size(); entryIdx++) {
        AnimationEntry& entry = anim.entries[entryIdx];

        float celStartX = frameIndex * frameWidth;
        float celWidth = entry.duration * frameWidth;

        if (celStartX - syncScroll + celWidth < 0 || celStartX - syncScroll > ImGui::GetWindowWidth()) {
            frameIndex += entry.duration;
            continue;
        }

        bool isSelected = std::find(selectedEntryIndices.begin(), selectedEntryIndices.end(), entryIdx) != selectedEntryIndices.end();

        drawTimelineEntryBackground(drawList, winPos, syncScroll, entryIdx, celStartX, celWidth, isSelected, selectedEntryIndices, timelineHoveredEntryIdx, clipboardEntries, entryHeight);
        handleTimelineEntryEdges(drawList, winPos, syncScroll, entryIdx, celStartX, celWidth,
            resizeState, frameIndex, entry, entryHeight);
        drawTimelineEntryLabel(drawList, winPos, syncScroll, entry.celName, celStartX, celWidth, entryHeight);
        drawTimelineFrameCels(drawList, winPos, syncScroll, frameIndex, entry.duration, frameWidth, entryHeight);

        frameIndex += entry.duration;
    }
}

void Sofanthiel::drawTimelineEntryBackground(ImDrawList* drawList, const ImVec2& winPos,
    float syncScroll, int entryIdx, float celStartX, float celWidth,
    bool isSelected, std::vector<int>& selectedEntryIndices, int& hoveredEntryIdx,
    std::vector<AnimationEntry>& clipboardEntries, float entryHeight)
{
    ImU32 entryColor;

    if (isSelected) {
        entryColor = IM_COL32(100, 150, 250, 180);
    }
    else {
        entryColor = IM_COL32(50 + (entryIdx * 70) % 150, 100 + (entryIdx * 50) % 150, 150 + (entryIdx * 40) % 100, 180);
    }

    drawList->AddRectFilled(
        ImVec2(winPos.x + celStartX - syncScroll, winPos.y),
        ImVec2(winPos.x + celStartX - syncScroll + celWidth, winPos.y + entryHeight),
        entryColor
    );

    drawList->AddRect(
        ImVec2(winPos.x + celStartX - syncScroll, winPos.y),
        ImVec2(winPos.x + celStartX - syncScroll + celWidth, winPos.y + entryHeight),
        isSelected ? IM_COL32(255, 255, 255, 200) : IM_COL32(200, 200, 200, 100)
    );

    ImVec2 celMin(winPos.x + celStartX - syncScroll, winPos.y);
    ImVec2 celMax(winPos.x + celStartX - syncScroll + celWidth, winPos.y + entryHeight);
    ImGui::SetCursorScreenPos(celMin);
    char entryButtonId[32];
    snprintf(entryButtonId, sizeof(entryButtonId), "##entry_dnd_%d", entryIdx);
    ImGui::InvisibleButton(entryButtonId, ImVec2(celWidth, entryHeight));
    if (ImGui::IsItemHovered()) {
        hoveredEntryIdx = entryIdx;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (ImGui::GetIO().KeyCtrl) {
            auto it = std::find(selectedEntryIndices.begin(), selectedEntryIndices.end(), entryIdx);
            if (it != selectedEntryIndices.end()) {
                selectedEntryIndices.erase(it);
            }
            else {
                selectedEntryIndices.push_back(entryIdx);
            }
        }
        else if (ImGui::GetIO().KeyShift && !selectedEntryIndices.empty()) {
            int lastSelected = selectedEntryIndices.back();
            int start = std::min(lastSelected, entryIdx);
            int end = std::max(lastSelected, entryIdx);

            for (int i = start; i <= end; i++) {
                if (std::find(selectedEntryIndices.begin(), selectedEntryIndices.end(), i) == selectedEntryIndices.end()) {
                    selectedEntryIndices.push_back(i);
                }
            }
        }
        else {
            selectedEntryIndices.clear();
            selectedEntryIndices.push_back(entryIdx);
        }
    }

    if (ImGui::BeginPopupContextItem(entryButtonId)) {
        if (currentAnimation >= 0 && currentAnimation < animations.size()) {
            Animation& anim = animations[currentAnimation];
            if (!isSelected) {
                selectedEntryIndices.clear();
                selectedEntryIndices.push_back(entryIdx);
            }
            if (ImGui::MenuItem("Copy", InputManager::Copy.label.c_str(), false, !selectedEntryIndices.empty())) {
                clipboardEntries.clear();
                for (int idx : selectedEntryIndices) {
                    if (idx >= 0 && idx < anim.entries.size()) {
                        clipboardEntries.push_back(anim.entries[idx]);
                    }
                }
            }

            if (ImGui::MenuItem("Paste", InputManager::Paste.label.c_str(), false, !clipboardEntries.empty())) {
                int insertPos = *std::max_element(selectedEntryIndices.begin(), selectedEntryIndices.end()) + 1;

                anim.entries.insert(anim.entries.begin() + insertPos,
                    clipboardEntries.begin(), clipboardEntries.end());

                selectedEntryIndices.clear();
                for (size_t i = 0; i < clipboardEntries.size(); i++) {
                    selectedEntryIndices.push_back(insertPos + i);
                }

                recalculateTotalFrames();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", InputManager::Delete.label.c_str(), false, !selectedEntryIndices.empty())) {
                std::sort(selectedEntryIndices.begin(), selectedEntryIndices.end(), std::greater<int>());

                for (int idx : selectedEntryIndices) {
                    if (idx >= 0 && idx < anim.entries.size()) {
                        anim.entries.erase(anim.entries.begin() + idx);
                    }
                }

                selectedEntryIndices.clear();

                recalculateTotalFrames();
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ANIM_CELL")) {
            if (payload->DataSize == sizeof(int)) {
                int celIdx = *(const int*)payload->Data;
                if (celIdx >= 0 && celIdx < animationCels.size() &&
                    currentAnimation >= 0 && currentAnimation < animations.size()) {
                    Animation& anim = animations[currentAnimation];
                    if (entryIdx >= 0 && entryIdx < anim.entries.size()) {
                        anim.entries[entryIdx].celName = animationCels[celIdx].name;
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void Sofanthiel::handleTimelineEntryEdges(ImDrawList* drawList, const ImVec2& winPos, float syncScroll,
    int entryIdx, float celStartX, float celWidth,
    TimelineResizeState& resizeState, int frameIndex, AnimationEntry& entry, float entryHeight)
{
    ImVec2 mousePos = ImGui::GetMousePos();

    bool isHoveringLeftEdge = mousePos.x >= winPos.x + celStartX - syncScroll - 3 &&
        mousePos.x <= winPos.x + celStartX - syncScroll + 3 &&
        mousePos.y >= winPos.y &&
        mousePos.y <= winPos.y + entryHeight;

    bool isHoveringRightEdge = mousePos.x >= winPos.x + celStartX - syncScroll + celWidth - 3 &&
        mousePos.x <= winPos.x + celStartX - syncScroll + celWidth + 3 &&
        mousePos.y >= winPos.y &&
        mousePos.y <= winPos.y + entryHeight;

    if (isHoveringLeftEdge && entryIdx > 0) {
        drawList->AddRectFilled(
            ImVec2(winPos.x + celStartX - syncScroll - 1, winPos.y),
            ImVec2(winPos.x + celStartX - syncScroll + 1, winPos.y + entryHeight),
            IM_COL32(255, 255, 255, 200)
        );
    }

    if (isHoveringRightEdge) {
        drawList->AddRectFilled(
            ImVec2(winPos.x + celStartX - syncScroll + celWidth - 1, winPos.y),
            ImVec2(winPos.x + celStartX - syncScroll + celWidth + 1, winPos.y + entryHeight),
            IM_COL32(255, 255, 255, 200)
        );
    }

    if ((isHoveringLeftEdge && entryIdx > 0) || isHoveringRightEdge) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !resizeState.isResizing) {
            resizeState.isResizing = true;
            resizeState.resizingEntryIdx = entryIdx;
            resizeState.resizingRight = isHoveringRightEdge;
            resizeState.resizeStartPos = mousePos.x;
            resizeState.originalDuration = entry.duration;
            resizeState.frameStartOffset = frameIndex;
        }
    }
}

void Sofanthiel::drawTimelineEntryLabel(ImDrawList* drawList, const ImVec2& winPos, float syncScroll,
    const std::string& celName, float celStartX, float celWidth, float entryHeight)
{
    std::string displayName = celName;
    bool isNameTruncated = false;
    int wantedSize = celWidth - 8;
	int currentSize = ImGui::CalcTextSize(displayName.c_str()).x;
    if (currentSize - 8 > celWidth) {
        while(currentSize > wantedSize && displayName.length() > 2) {
            displayName = displayName.substr(0, displayName.length() - 1);
            currentSize = ImGui::CalcTextSize(displayName.c_str()).x;
		}
        isNameTruncated = true;
    }

    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
    float textX = winPos.x + celStartX - syncScroll + (celWidth - textSize.x) * 0.5f;
    float textY = winPos.y + (entryHeight - textSize.y) * 0.5f;

    drawList->AddText(
        ImVec2(textX + 1, textY + 1),
        IM_COL32(0, 0, 0, 200),
        displayName.c_str()
    );

    drawList->AddText(
        ImVec2(textX, textY),
        IM_COL32(255, 255, 255, 255),
        displayName.c_str()
    );
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    ImVec2 celMin = ImVec2(winPos.x + celStartX - syncScroll, winPos.y);
    ImVec2 celMax = ImVec2(celMin.x + celWidth, celMin.y + entryHeight);

    if (mousePos.x >= celMin.x && mousePos.x <= celMax.x &&
        mousePos.y >= celMin.y && mousePos.y <= celMax.y && isNameTruncated) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", celName.c_str());
        ImGui::EndTooltip();
    }
}

void Sofanthiel::drawTimelineFrameCels(ImDrawList* drawList, const ImVec2& winPos, float syncScroll,
    int frameStartIndex, int duration, float frameWidth, float entryHeight)
{
    for (int i = 0; i < duration; i++) {
        int currentCelFrame = frameStartIndex + i;
        float frameCelX = currentCelFrame * frameWidth;

        if (frameCelX - syncScroll < -frameWidth || frameCelX - syncScroll > ImGui::GetWindowWidth())
            continue;

        ImGui::SetCursorPos(ImVec2(frameCelX, 0));

        char frameLabel[32];
        snprintf(frameLabel, sizeof(frameLabel), "##frame_%d", currentCelFrame);

        ImGui::PushStyleColor(ImGuiCol_Button,
            (currentCelFrame == currentFrame) ?
            ImVec4(1.0f, 0.5f, 0.0f, 0.7f) :
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

        if (ImGui::Button(frameLabel, ImVec2(frameWidth - 1, entryHeight))) {
            currentFrame = currentCelFrame;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

void Sofanthiel::handleTimelineResizing(Animation& anim, TimelineResizeState& resizeState,
    float syncScroll, float frameWidth)
{
    if (resizeState.isResizing && resizeState.resizingEntryIdx >= 0 &&
        resizeState.resizingEntryIdx < anim.entries.size()) {

        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            AnimationEntry& entry = anim.entries[resizeState.resizingEntryIdx];
            float dragDelta = ImGui::GetMousePos().x - resizeState.resizeStartPos;
            int frameDelta = (int)(dragDelta / frameWidth);

            if (resizeState.resizingRight) {
                int newDuration = resizeState.originalDuration + frameDelta;
                entry.duration = std::max(1, newDuration);
            }
            else {
                if (resizeState.resizingEntryIdx > 0) {
                    AnimationEntry& prevEntry = anim.entries[resizeState.resizingEntryIdx - 1];
                    int newPrevDuration = prevEntry.duration + frameDelta;
                    int newCurrDuration = entry.duration - frameDelta;

                    if (newPrevDuration >= 1 && newCurrDuration >= 1) {
                        prevEntry.duration = newPrevDuration;
                        entry.duration = newCurrDuration;
                        resizeState.resizeStartPos = ImGui::GetMousePos().x;
                    }
                }
            }

            recalculateTotalFrames();
        }
        else {
            resizeState.isResizing = false;
            resizeState.resizingEntryIdx = -1;
        }
    }
}

void Sofanthiel::handleTimelineAutoScroll(float syncScroll, float frameWidth)
{
    if (isPlaying) {
        float currentFramePos = currentFrame * frameWidth;
        float windowWidth = ImGui::GetWindowWidth();
        float visibleStart = syncScroll;
        float visibleEnd = visibleStart + windowWidth;

        if (currentFramePos < visibleStart + 50.0f ||
            currentFramePos > visibleEnd - 100.0f) {
            ImGui::SetScrollX(currentFramePos - windowWidth * 0.5f);
        }
    }
}

void Sofanthiel::handleTimelineDragging(Animation& anim, ImDrawList* drawList, const ImVec2& winPos,
    float syncScroll, float frameWidth, TimelineDragState& dragState, float entryHeight)
{
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (!dragState.isDragging) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (mousePos.y >= winPos.y && mousePos.y <= winPos.y + entryHeight) {
                int frameIndex = 0;
                for (int entryIdx = 0; entryIdx < anim.entries.size(); entryIdx++) {
                    const AnimationEntry& entry = anim.entries[entryIdx];
                    float celStartX = frameIndex * frameWidth;
                    float celWidth = entry.duration * frameWidth;

                    float celLeft = winPos.x + celStartX - syncScroll;
                    float celRight = celLeft + celWidth;

                    if (mousePos.x >= celLeft && mousePos.x < celRight) {
                        dragState.draggedEntryIdx = entryIdx;
                        dragState.dragStartPosX = mousePos.x;
                        dragState.dragCurrentPosX = mousePos.x;
                        dragState.offsetFromEntryLeft = mousePos.x - celLeft;
                        dragState.targetInsertIdx = entryIdx;

                        float clickOffsetInCel = mousePos.x - celLeft;
                        int frameOffset = static_cast<int>(clickOffsetInCel / frameWidth);
                        currentFrame = frameIndex + std::min(frameOffset, entry.duration - 1);
                        break;
                    }

                    frameIndex += entry.duration;
                }
            }
        }
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && dragState.draggedEntryIdx >= 0) {
            float dragDelta = std::abs(ImGui::GetMousePos().x - dragState.dragStartPosX);
            if (dragDelta > 5.0f) {
                dragState.isDragging = true;
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            dragState.draggedEntryIdx = -1;
        }
    }
    else {
        dragState.dragCurrentPosX = mousePos.x;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        float mouseFramePos = (mousePos.x - winPos.x + syncScroll) / frameWidth;
        std::vector<std::pair<int, int>> framePositions;
        int framePos = 0;

        framePositions.push_back(std::make_pair(framePos, 0));
        for (int i = 0; i < anim.entries.size(); i++) {
            framePos += anim.entries[i].duration;
            framePositions.push_back(std::make_pair(framePos, i + 1));
        }

        int bestIdx = 0;
        float bestDist = FLT_MAX;
        for (const auto& pos : framePositions) {
            float dist = std::abs(mouseFramePos - pos.first);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = pos.second;
            }
        }

        dragState.targetInsertIdx = bestIdx;

        if (dragState.targetInsertIdx == dragState.draggedEntryIdx) {
            dragState.targetInsertIdx = -1;
        }

        if (dragState.draggedEntryIdx >= 0 && dragState.draggedEntryIdx < anim.entries.size()) {
            const AnimationEntry& entry = anim.entries[dragState.draggedEntryIdx];
            float celWidth = entry.duration * frameWidth;

            float draggedX = mousePos.x - dragState.offsetFromEntryLeft;

            ImU32 draggedColor = IM_COL32(100, 150, 250, 200);
            drawList->AddRectFilled(
                ImVec2(draggedX, winPos.y),
                ImVec2(draggedX + celWidth, winPos.y + entryHeight),
                draggedColor
            );

            drawList->AddRect(
                ImVec2(draggedX, winPos.y),
                ImVec2(draggedX + celWidth, winPos.y + entryHeight),
                IM_COL32(200, 220, 255, 230),
                0.0f, 0, 2.0f
            );

            if (celWidth > getScaledSize(30.0f)) {
                std::string displayName = entry.celName;
                if (displayName.length() > 8 && celWidth < getScaledSize(80.0f))
                    displayName = displayName.substr(0, 6) + "..";

                ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
                float textX = draggedX + (celWidth - textSize.x) * 0.5f;
                float textY = winPos.y + (entryHeight - textSize.y) * 0.5f;

                drawList->AddText(
                    ImVec2(textX + 1, textY + 1),
                    IM_COL32(0, 0, 0, 200),
                    displayName.c_str()
                );

                drawList->AddText(
                    ImVec2(textX, textY),
                    IM_COL32(255, 255, 255, 255),
                    displayName.c_str()
                );
            }

            if (dragState.targetInsertIdx >= 0) {
                int insertFramePos = 0;
                for (int i = 0; i < dragState.targetInsertIdx && i < anim.entries.size(); i++) {
                    insertFramePos += anim.entries[i].duration;
                }

                float insertX = winPos.x + insertFramePos * frameWidth - syncScroll;

                drawList->AddLine(
                    ImVec2(insertX, winPos.y - 2),
                    ImVec2(insertX, winPos.y + entryHeight),
                    IM_COL32(255, 255, 0, 255),
                    2.0f
                );

                const float triangleSize = 6.0f;
                ImVec2 trianglePoints[3];

                trianglePoints[0] = ImVec2(insertX - triangleSize, winPos.y - 2);
                trianglePoints[1] = ImVec2(insertX + triangleSize, winPos.y - 2);
                trianglePoints[2] = ImVec2(insertX, winPos.y - 2 + triangleSize);
                drawList->AddTriangleFilled(
                    trianglePoints[0], trianglePoints[1], trianglePoints[2],
                    IM_COL32(255, 255, 0, 255)
                );

                trianglePoints[0] = ImVec2(insertX - triangleSize, winPos.y + entryHeight);
                trianglePoints[1] = ImVec2(insertX + triangleSize, winPos.y + entryHeight);
                trianglePoints[2] = ImVec2(insertX, winPos.y + entryHeight - triangleSize);
                drawList->AddTriangleFilled(
                    trianglePoints[0], trianglePoints[1], trianglePoints[2],
                    IM_COL32(255, 255, 0, 255)
                );
            }
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (dragState.targetInsertIdx >= 0 &&
                dragState.draggedEntryIdx >= 0 &&
                dragState.draggedEntryIdx < anim.entries.size() &&
                dragState.targetInsertIdx <= anim.entries.size()) {

                AnimationEntry movedEntry = anim.entries[dragState.draggedEntryIdx];

                anim.entries.erase(anim.entries.begin() + dragState.draggedEntryIdx);

                int targetIdx = dragState.targetInsertIdx;
                if (dragState.draggedEntryIdx < targetIdx)
                    targetIdx--;

                targetIdx = SDL_clamp(targetIdx, 0, static_cast<int>(anim.entries.size()));

                anim.entries.insert(anim.entries.begin() + targetIdx, movedEntry);

                recalculateTotalFrames();
            }

            dragState.isDragging = false;
            dragState.draggedEntryIdx = -1;
            dragState.targetInsertIdx = -1;
        }
    }
}
