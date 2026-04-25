#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"
#include <cfloat>
#include <cmath>

ImU32 getTimelineEntryColor(int entryIdx, bool isSelected)
{
    if (isSelected) {
        return IM_COL32(100, 150, 250, 180);
    }

    return IM_COL32(
        50 + (entryIdx * 70) % 150,
        100 + (entryIdx * 50) % 150,
        150 + (entryIdx * 40) % 100,
        180);
}

ImRect getTimelineEntryRect(const ImVec2& winPos, float syncScroll, float celStartX, float celWidth, float entryHeight)
{
    return ImRect(
        ImVec2(winPos.x + celStartX - syncScroll, winPos.y),
        ImVec2(winPos.x + celStartX - syncScroll + celWidth, winPos.y + entryHeight));
}

float getTimelineHandleWidth(float celWidth, float entryHeight)
{
    if (celWidth <= 0.0f) {
        return 0.0f;
    }

    float preferredWidth = entryHeight * 0.48f;
    float maxWidth = ImMax(7.0f, celWidth * 0.38f);
    return ImClamp(preferredWidth, 7.0f, maxWidth);
}

ImRect getTimelineHandleRect(const ImRect& entryRect, float entryHeight)
{
    float handleWidth = getTimelineHandleWidth(entryRect.GetWidth(), entryHeight);
    float inset = ImMin(4.0f, entryRect.GetWidth() * 0.2f);
    float minX = ImMin(entryRect.Min.x + inset, entryRect.Max.x);
    float maxX = ImMin(entryRect.Max.x, minX + handleWidth);
    return ImRect(ImVec2(minX, entryRect.Min.y), ImVec2(maxX, entryRect.Max.y));
}

void drawTimelineEntryBody(ImDrawList* drawList, const ImRect& entryRect, ImU32 fillColor, ImU32 borderColor)
{
    drawList->AddRectFilled(entryRect.Min, entryRect.Max, fillColor);
    drawList->AddRect(entryRect.Min, entryRect.Max, borderColor);
}

void drawTimelineHandle(ImDrawList* drawList, const ImRect& handleRect, bool isHovered, bool isActive)
{
    ImU32 handleFill = isActive ? IM_COL32(255, 220, 120, 110) :
        isHovered ? IM_COL32(255, 255, 255, 55) :
        IM_COL32(0, 0, 0, 45);
    ImU32 lineColor = isActive ? IM_COL32(255, 245, 220, 255) :
        isHovered ? IM_COL32(255, 255, 255, 230) :
        IM_COL32(255, 255, 255, 160);

    ImRect gripRect(
        ImVec2(handleRect.Min.x + 1.0f, handleRect.Min.y + 2.0f),
        ImVec2(handleRect.Max.x - 1.0f, handleRect.Max.y - 2.0f));
    if (gripRect.GetWidth() <= 0.0f || gripRect.GetHeight() <= 0.0f) {
        gripRect = handleRect;
    }

    drawList->AddRectFilled(gripRect.Min, gripRect.Max, handleFill, 2.0f);

    float centerX = (gripRect.Min.x + gripRect.Max.x) * 0.5f;
    float top = gripRect.Min.y + 4.0f;
    float bottom = gripRect.Max.y - 4.0f;
    float spacing = ImClamp(gripRect.GetWidth() * 0.17f, 2.0f, 2.6f);
    for (int i = -1; i <= 1; ++i) {
        float x = centerX + i * spacing;
        drawList->AddLine(ImVec2(x, top), ImVec2(x, bottom), lineColor, 1.0f);
    }
}

int getTimelineEntryStartFrame(const Animation& anim, int entryIdx)
{
    int frameStart = 0;
    int clampedIdx = SDL_clamp(entryIdx, 0, static_cast<int>(anim.entries.size()));
    for (int i = 0; i < clampedIdx; ++i) {
        frameStart += anim.entries[i].duration;
    }
    return frameStart;
}

std::vector<int> getNormalizedTimelineEntryIndices(const std::vector<int>& indices, int entryCount)
{
    std::vector<int> normalized;
    normalized.reserve(indices.size());
    for (int idx : indices) {
        if (idx >= 0 && idx < entryCount) {
            normalized.push_back(idx);
        }
    }

    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
    return normalized;
}

bool isTimelineEntryIndexInList(const std::vector<int>& indices, int entryIdx)
{
    return std::binary_search(indices.begin(), indices.end(), entryIdx);
}

std::vector<int> getTimelineDraggedEntryIndices(const std::vector<int>& selectedEntryIndices,
    int draggedEntryIdx, int entryCount)
{
    std::vector<int> draggedEntryIndices = getNormalizedTimelineEntryIndices(selectedEntryIndices, entryCount);
    if (!isTimelineEntryIndexInList(draggedEntryIndices, draggedEntryIdx)) {
        draggedEntryIndices.clear();
        if (draggedEntryIdx >= 0 && draggedEntryIdx < entryCount) {
            draggedEntryIndices.push_back(draggedEntryIdx);
        }
    }

    return draggedEntryIndices;
}

float getTimelineDraggedWidth(const Animation& anim, const std::vector<int>& draggedEntryIndices, float frameWidth)
{
    float draggedWidth = 0.0f;
    for (int entryIdx : draggedEntryIndices) {
        draggedWidth += anim.entries[entryIdx].duration * frameWidth;
    }
    return draggedWidth;
}

float getTimelineDraggedBlockOffset(const Animation& anim, const std::vector<int>& draggedEntryIndices,
    int draggedEntryIdx, float frameWidth)
{
    float offset = 0.0f;
    for (int entryIdx : draggedEntryIndices) {
        if (entryIdx == draggedEntryIdx) {
            break;
        }
        offset += anim.entries[entryIdx].duration * frameWidth;
    }
    return offset;
}

int getTimelinePreviewInsertIdx(const std::vector<int>& draggedEntryIndices, int rawInsertIdx, int entryCount)
{
    if (draggedEntryIndices.empty()) {
        return -1;
    }

    int insertIdx = SDL_clamp(rawInsertIdx, 0, entryCount);
    for (int draggedEntryIdx : draggedEntryIndices) {
        if (draggedEntryIdx < insertIdx) {
            insertIdx--;
        }
    }

    return SDL_clamp(insertIdx, 0, entryCount - static_cast<int>(draggedEntryIndices.size()));
}

std::vector<int> buildTimelinePreviewOrder(int entryCount, const std::vector<int>& draggedEntryIndices,
    int previewInsertIdx)
{
    std::vector<int> previewOrder;
    previewOrder.reserve(entryCount);

    int clampedInsertIdx = SDL_clamp(
        previewInsertIdx,
        0,
        entryCount - static_cast<int>(draggedEntryIndices.size()));

    bool insertedDraggedEntries = false;
    int remainingEntryIdx = 0;
    for (int entryIdx = 0; entryIdx < entryCount; ++entryIdx) {
        if (isTimelineEntryIndexInList(draggedEntryIndices, entryIdx)) {
            continue;
        }

        if (!insertedDraggedEntries && remainingEntryIdx == clampedInsertIdx) {
            previewOrder.insert(previewOrder.end(), draggedEntryIndices.begin(), draggedEntryIndices.end());
            insertedDraggedEntries = true;
        }

        previewOrder.push_back(entryIdx);
        ++remainingEntryIdx;
    }

    if (!insertedDraggedEntries) {
        previewOrder.insert(previewOrder.end(), draggedEntryIndices.begin(), draggedEntryIndices.end());
    }

    return previewOrder;
}

bool isTimelinePreviewOrderOriginal(const std::vector<int>& previewOrder)
{
    for (int i = 0; i < static_cast<int>(previewOrder.size()); ++i) {
        if (previewOrder[i] != i) {
            return false;
        }
    }

    return true;
}

int getTimelineRawInsertIdx(const Animation& anim, float probeFramePos)
{
    std::vector<std::pair<int, int>> framePositions;
    framePositions.reserve(anim.entries.size() + 1);

    int framePos = 0;
    framePositions.push_back(std::make_pair(framePos, 0));
    for (int i = 0; i < static_cast<int>(anim.entries.size()); ++i) {
        framePos += anim.entries[i].duration;
        framePositions.push_back(std::make_pair(framePos, i + 1));
    }

    int bestIdx = 0;
    float bestDist = FLT_MAX;
    for (const auto& pos : framePositions) {
        float dist = std::abs(probeFramePos - static_cast<float>(pos.first));
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = pos.second;
        }
    }

    return bestIdx;
}

int getTimelineVisibleFrameStart(float syncScroll, float frameWidth)
{
    if (frameWidth <= 0.0f) {
        return 0;
    }

    return ImMax(0, static_cast<int>(std::floor(syncScroll / frameWidth)) - 1);
}

int getTimelineVisibleFrameEnd(float syncScroll, float frameWidth, float windowWidth, int totalFrames)
{
    if (frameWidth <= 0.0f) {
        return 0;
    }

    int lastVisible = static_cast<int>(std::ceil((syncScroll + windowWidth) / frameWidth)) + 1;
    return ImClamp(lastVisible, 0, totalFrames);
}

int getTimelineLabelStep(float frameWidth)
{
    const float minSpacing = 56.0f;
    int step = 1;

    while (step * frameWidth < minSpacing) {
        if (step == 1) {
            step = 2;
        }
        else if (step == 2) {
            step = 5;
        }
        else {
            step *= 2;
        }
    }

    return step;
}

void drawTimelineFrameGrid(ImDrawList* drawList, const ImVec2& winPos, float syncScroll,
    float frameWidth, float height, int totalFrames)
{
    if (frameWidth <= 0.0f || height <= 0.0f || totalFrames <= 0) {
        return;
    }

    float windowWidth = ImGui::GetWindowWidth();
    int firstFrame = getTimelineVisibleFrameStart(syncScroll, frameWidth);
    int lastFrame = getTimelineVisibleFrameEnd(syncScroll, frameWidth, windowWidth, totalFrames);

    for (int frame = firstFrame; frame <= lastFrame; ++frame) {
        float x = winPos.x + frame * frameWidth - syncScroll;
        bool majorLine = (frame % 10) == 0;
        bool midLine = (frame % 5) == 0;
        ImU32 lineColor = majorLine ? IM_COL32(255, 255, 255, 42) :
            midLine ? IM_COL32(255, 255, 255, 24) :
            IM_COL32(255, 255, 255, 12);

        drawList->AddLine(
            ImVec2(x, winPos.y),
            ImVec2(x, winPos.y + height),
            lineColor,
            majorLine ? 1.35f : 1.0f);
    }
}

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

    float baseFrameWidth = getScaledSize(15.0f);
    float previousFrameWidth = baseFrameWidth * timelineHorizontalZoom;
    drawTimelineControls();
    timelineHorizontalZoom = ImClamp(timelineHorizontalZoom, 0.6f, 6.0f);
    float frameWidth = baseFrameWidth * timelineHorizontalZoom;
    if (previousFrameWidth > 0.0f && std::abs(previousFrameWidth - frameWidth) > 0.01f) {
        syncScroll = ImMax(0.0f, (syncScroll / previousFrameWidth) * frameWidth);
    }

    ImGui::Separator();

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

    ImGui::Text("Frame Width");
    ImGui::SameLine();

    float zoomStep = 0.15f;
    if (ImGui::SmallButton("-")) {
        timelineHorizontalZoom = ImMax(0.6f, timelineHorizontalZoom - zoomStep);
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(getScaledSize(150.0f));
    ImGui::SliderFloat("##TimelineHorizontalZoom", &timelineHorizontalZoom, 0.6f, 6.0f, "%.2fx");
    ImGui::SameLine();

    if (ImGui::SmallButton("+")) {
        timelineHorizontalZoom = ImMin(6.0f, timelineHorizontalZoom + zoomStep);
    }
    ImGui::SameLine();

    if (ImGui::Button("1:1")) {
        timelineHorizontalZoom = 1.0f;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+Wheel in timeline");
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

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    drawTimelineFrameGrid(drawList, winPos, syncScroll, frameWidth, headerHeight, totalFrames);

    int labelStep = getTimelineLabelStep(frameWidth);
    int firstFrame = getTimelineVisibleFrameStart(syncScroll, frameWidth);
    int lastFrame = getTimelineVisibleFrameEnd(syncScroll, frameWidth, ImGui::GetWindowWidth(), totalFrames);
    int firstLabel = (firstFrame / labelStep) * labelStep;

    for (int i = firstLabel; i <= lastFrame; i += labelStep) {
        float xPos = i * frameWidth - syncScroll + getScaledSize(3.0f);
        ImGui::SetCursorPosX(xPos);
        ImGui::Text("%d", i);
        ImGui::SameLine();
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

    handleTimelineScrolling(syncScroll, frameWidth);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();

    if (showGrid) {
        drawTimelineFrameGrid(drawList, winPos, syncScroll, frameWidth, ImGui::GetWindowHeight(), totalFrames);
    }

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
                int insertPos = timelineSelectedEntryIndices.empty() ? static_cast<int>(anim.entries.size()) :
                    *std::max_element(timelineSelectedEntryIndices.begin(), timelineSelectedEntryIndices.end()) + 1;

                auto oldEntries = anim.entries;

                anim.entries.insert(anim.entries.begin() + insertPos,
                    clipboardEntries.begin(), clipboardEntries.end());

                timelineSelectedEntryIndices.clear();
                for (int i = 0; i < static_cast<int>(clipboardEntries.size()); ++i) {
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

        handleTimelineResizing(anim, timelineResizeState, syncScroll, frameWidth);
        if (!timelineResizeState.isResizing) {
            handleTimelineDragging(anim, winPos, syncScroll, frameWidth, timelineDragState, timelineEntryHeight);
        }

        if (!timelineDragState.isDragging) {
            drawTimelineEntries(anim, drawList, winPos, syncScroll, frameWidth, timelineResizeState, timelineSelectedEntryIndices, timelineEntryHeight, clipboardEntries);
        }
        else {
            drawTimelineDragPreview(anim, drawList, winPos, syncScroll, frameWidth, timelineDragState, timelineSelectedEntryIndices, timelineEntryHeight);
        }

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

void Sofanthiel::handleTimelineScrolling(float& syncScroll, float& frameWidth)
{
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowHovered() && io.KeyCtrl && io.MouseWheel != 0.0f) {
        float oldFrameWidth = frameWidth;
        float oldZoom = timelineHorizontalZoom;
        timelineHorizontalZoom = ImClamp(timelineHorizontalZoom + io.MouseWheel * 0.10f, 0.6f, 6.0f);

        if (std::abs(oldZoom - timelineHorizontalZoom) > 0.001f && oldFrameWidth > 0.0f) {
            frameWidth = getScaledSize(15.0f) * timelineHorizontalZoom;

            float mouseLocalX = io.MousePos.x - ImGui::GetWindowPos().x;
            float anchoredFrame = (mouseLocalX + syncScroll) / oldFrameWidth;
            float newScroll = anchoredFrame * frameWidth - mouseLocalX;
            syncScroll = ImClamp(newScroll, 0.0f, ImGui::GetScrollMaxX());
            ImGui::SetScrollX(syncScroll);
        }

        io.MouseWheel = 0.0f;
    }
    else if (ImGui::IsWindowHovered() && io.MouseWheel != 0 && !io.KeyShift && !io.KeyCtrl) {
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

        drawTimelineEntryBackground(drawList, winPos, syncScroll, entryIdx, celStartX, celWidth, isSelected, selectedEntryIndices, timelineHoveredEntryIdx, clipboardEntries, entryHeight, frameWidth);
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
    std::vector<AnimationEntry>& clipboardEntries, float entryHeight, int frameWidth)
{
    ImRect entryRect = getTimelineEntryRect(winPos, syncScroll, celStartX, celWidth, entryHeight);
    ImRect handleRect = getTimelineHandleRect(entryRect, entryHeight);
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isHandleHovered = handleRect.Contains(mousePos);
    bool isHandleActive = isTimelineEntryIndexInList(timelineDragState.draggedEntryIndices, entryIdx);

    drawTimelineEntryBody(
        drawList,
        entryRect,
        getTimelineEntryColor(entryIdx, isSelected),
        isSelected ? IM_COL32(255, 255, 255, 200) : IM_COL32(200, 200, 200, 100));
    drawTimelineHandle(drawList, handleRect, isHandleHovered, isHandleActive);

    ImGui::SetCursorScreenPos(entryRect.Min);
    char entryButtonId[32];
    snprintf(entryButtonId, sizeof(entryButtonId), "##entry_dnd_%d", entryIdx);
    ImGui::InvisibleButton(entryButtonId, ImVec2(celWidth, entryHeight));
    if (ImGui::IsItemHovered()) {
        hoveredEntryIdx = entryIdx;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        bool preserveSelectionForHandleDrag = isHandleHovered &&
            isSelected &&
            !ImGui::GetIO().KeyCtrl &&
            !ImGui::GetIO().KeyShift;

        if (!isHandleHovered) {
            int celFrameStart = static_cast<int>(std::round(celStartX / frameWidth));
            float mouseX = ImGui::GetIO().MousePos.x;
            float localX = mouseX - (winPos.x + celStartX - syncScroll);
            int clickedFrameOffset = static_cast<int>(localX / frameWidth);
            int celDuration = static_cast<int>(std::round(celWidth / frameWidth));
            currentFrame = celFrameStart + ImClamp(clickedFrameOffset, 0, celDuration - 1);
        }

        if (!preserveSelectionForHandleDrag) {
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
    }

    if (isHandleHovered && !timelineDragState.isDragging && !timelineResizeState.isResizing) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(
            isSelected && selectedEntryIndices.size() > 1
            ? "Drag the grip to move the selected entries"
            : "Drag the grip to move this entry");
        ImGui::EndTooltip();
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (currentAnimation >= 0 && currentAnimation < static_cast<int>(animations.size()) &&
            entryIdx >= 0 && entryIdx < static_cast<int>(animations[currentAnimation].entries.size())) {
            const std::string& celName = animations[currentAnimation].entries[entryIdx].celName;
            for (int celIdx = 0; celIdx < static_cast<int>(animationCels.size()); celIdx++) {
                if (animationCels[celIdx].name == celName) {
                    celEditingMode = true;
                    editingCelIndex = celIdx;
                    selectedOAMIndices.clear();
                    break;
                }
            }
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
                for (int i = 0; i < static_cast<int>(clipboardEntries.size()); ++i) {
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
    float leftPadding = getTimelineHandleWidth(celWidth, entryHeight) + 8.0f;
    float wantedSize = celWidth - leftPadding - 4.0f;
	float currentSize = ImGui::CalcTextSize(displayName.c_str()).x;
    if (wantedSize <= 0.0f) {
        return;
    }

    if (currentSize > wantedSize) {
        while(currentSize > wantedSize && displayName.length() > 2) {
            displayName = displayName.substr(0, displayName.length() - 1);
            currentSize = ImGui::CalcTextSize(displayName.c_str()).x;
		}
        isNameTruncated = true;
    }

    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
    float labelMinX = winPos.x + celStartX - syncScroll + leftPadding;
    float labelMaxX = winPos.x + celStartX - syncScroll + celWidth - 4.0f;
    float labelWidth = labelMaxX - labelMinX;
    if (labelWidth <= 0.0f) {
        return;
    }

    float textX = labelMinX + (labelWidth - textSize.x) * 0.5f;
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

void Sofanthiel::drawTimelineDragPreview(Animation& anim, ImDrawList* drawList, const ImVec2& winPos,
    float syncScroll, float frameWidth, const TimelineDragState& dragState,
    const std::vector<int>& selectedEntryIndices, float entryHeight)
{
    if (dragState.draggedEntryIdx < 0 || dragState.draggedEntryIdx >= static_cast<int>(anim.entries.size())) {
        return;
    }

    std::vector<int> draggedEntryIndices = dragState.draggedEntryIndices.empty()
        ? getTimelineDraggedEntryIndices(selectedEntryIndices, dragState.draggedEntryIdx, static_cast<int>(anim.entries.size()))
        : dragState.draggedEntryIndices;
    if (draggedEntryIndices.empty()) {
        return;
    }

    const int previewInsertIdx = getTimelinePreviewInsertIdx(
        draggedEntryIndices,
        dragState.targetInsertIdx,
        static_cast<int>(anim.entries.size()));
    if (previewInsertIdx < 0) {
        return;
    }

    std::vector<int> previewOrder = buildTimelinePreviewOrder(
        static_cast<int>(anim.entries.size()),
        draggedEntryIndices,
        previewInsertIdx);
    float draggedWidth = getTimelineDraggedWidth(anim, draggedEntryIndices, frameWidth);

    int frameIndex = 0;
    int previewFrameStart = 0;
    bool hasPreviewFrameStart = false;
    for (int idx : previewOrder) {
        const AnimationEntry& entry = anim.entries[idx];
        bool isDraggedEntry = isTimelineEntryIndexInList(draggedEntryIndices, idx);
        if (isDraggedEntry) {
            if (!hasPreviewFrameStart) {
                previewFrameStart = frameIndex;
                hasPreviewFrameStart = true;
            }
            frameIndex += entry.duration;
            continue;
        }

        float celStartX = frameIndex * frameWidth;
        float celWidth = entry.duration * frameWidth;
        bool isVisible = !(celStartX - syncScroll + celWidth < 0 || celStartX - syncScroll > ImGui::GetWindowWidth());
        if (isVisible) {
            bool isSelected = std::find(selectedEntryIndices.begin(), selectedEntryIndices.end(), idx) != selectedEntryIndices.end();
            ImRect entryRect = getTimelineEntryRect(winPos, syncScroll, celStartX, celWidth, entryHeight);
            drawTimelineEntryBody(
                drawList,
                entryRect,
                getTimelineEntryColor(idx, isSelected),
                isSelected ? IM_COL32(255, 255, 255, 200) : IM_COL32(200, 200, 200, 100));
            drawTimelineHandle(drawList, getTimelineHandleRect(entryRect, entryHeight), false, false);
            drawTimelineEntryLabel(drawList, winPos, syncScroll, entry.celName, celStartX, celWidth, entryHeight);
        }

        frameIndex += entry.duration;
    }

    if (!hasPreviewFrameStart) {
        previewFrameStart = frameIndex;
    }

    bool isNoOp = isTimelinePreviewOrderOriginal(previewOrder);
    float targetStartX = previewFrameStart * frameWidth;

    ImRect targetRect = getTimelineEntryRect(winPos, syncScroll, targetStartX, draggedWidth, entryHeight);

    for (int idx : draggedEntryIndices) {
        float sourceStartX = getTimelineEntryStartFrame(anim, idx) * frameWidth;
        float sourceWidth = anim.entries[idx].duration * frameWidth;
        ImRect sourceRect = getTimelineEntryRect(winPos, syncScroll, sourceStartX, sourceWidth, entryHeight);
        drawList->AddRectFilled(sourceRect.Min, sourceRect.Max, IM_COL32(255, 255, 255, 18));
        drawList->AddRect(sourceRect.Min, sourceRect.Max, IM_COL32(255, 170, 90, 140), 0.0f, 0, 1.5f);
    }

    ImU32 targetFill = isNoOp ? IM_COL32(140, 190, 255, 55) : IM_COL32(255, 222, 120, 70);
    ImU32 targetBorder = isNoOp ? IM_COL32(170, 210, 255, 210) : IM_COL32(255, 232, 140, 255);
    drawList->AddRectFilled(targetRect.Min, targetRect.Max, targetFill);
    drawList->AddRect(targetRect.Min, targetRect.Max, targetBorder, 0.0f, 0, 2.0f);

    float sourceStartX = getTimelineEntryStartFrame(anim, draggedEntryIndices.front()) * frameWidth;
    float sourceEndX = sourceStartX;
    for (int idx : draggedEntryIndices) {
        sourceEndX += anim.entries[idx].duration * frameWidth;
    }
    ImRect sourceBounds = getTimelineEntryRect(
        winPos,
        syncScroll,
        sourceStartX,
        sourceEndX - sourceStartX,
        entryHeight);

    float sourceCenterX = (sourceBounds.Min.x + sourceBounds.Max.x) * 0.5f;
    float targetCenterX = (targetRect.Min.x + targetRect.Max.x) * 0.5f;
    if (std::abs(sourceCenterX - targetCenterX) > 1.0f) {
        float controlYOffset = getScaledSize(18.0f);
        drawList->AddBezierCubic(
            ImVec2(sourceCenterX, sourceBounds.Min.y - 3.0f),
            ImVec2(sourceCenterX, sourceBounds.Min.y - controlYOffset),
            ImVec2(targetCenterX, targetRect.Min.y - controlYOffset),
            ImVec2(targetCenterX, targetRect.Min.y - 3.0f),
            IM_COL32(255, 210, 120, 180),
            2.0f);
    }

    const char* indicatorLabel = isNoOp ? "Original position" : "Drop here";
    ImVec2 indicatorSize = ImGui::CalcTextSize(indicatorLabel);
    float indicatorX = targetRect.Min.x + (targetRect.GetWidth() - indicatorSize.x) * 0.5f;
    drawList->AddText(
        ImVec2(indicatorX, targetRect.Min.y - indicatorSize.y - 3.0f),
        targetBorder,
        indicatorLabel);

    float draggedX = dragState.dragCurrentPosX - dragState.offsetFromDraggedBlockLeft;
    ImRect draggedRect(ImVec2(draggedX, winPos.y), ImVec2(draggedX + draggedWidth, winPos.y + entryHeight));

    drawList->AddRectFilled(
        ImVec2(draggedRect.Min.x + 4.0f, draggedRect.Min.y + 4.0f),
        ImVec2(draggedRect.Max.x + 4.0f, draggedRect.Max.y + 4.0f),
        IM_COL32(0, 0, 0, 70),
        0.0f);

    float draggedEntryX = draggedX;
    for (int idx : draggedEntryIndices) {
        const AnimationEntry& entry = anim.entries[idx];
        float entryWidth = entry.duration * frameWidth;
        ImRect entryRect(
            ImVec2(draggedEntryX, winPos.y),
            ImVec2(draggedEntryX + entryWidth, winPos.y + entryHeight));

        drawTimelineEntryBody(
            drawList,
            entryRect,
            IM_COL32(100, 150, 250, 220),
            IM_COL32(220, 235, 255, 255));
        drawTimelineHandle(drawList, getTimelineHandleRect(entryRect, entryHeight), false, true);
        drawTimelineEntryLabel(
            drawList,
            winPos,
            syncScroll,
            entry.celName,
            draggedEntryX - winPos.x + syncScroll,
            entryWidth,
            entryHeight);

        draggedEntryX += entryWidth;
    }
}

void Sofanthiel::handleTimelineDragging(Animation& anim, const ImVec2& winPos,
    float& syncScroll, float frameWidth, TimelineDragState& dragState, float entryHeight)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    if (!dragState.isDragging) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            dragState.draggedEntryIdx = -1;
            dragState.draggedEntryIndices.clear();
            dragState.targetInsertIdx = -1;
            dragState.offsetFromDraggedBlockLeft = 0.0f;

            if (mousePos.y >= winPos.y && mousePos.y <= winPos.y + entryHeight) {
                int frameIndex = 0;
                for (int entryIdx = 0; entryIdx < static_cast<int>(anim.entries.size()); entryIdx++) {
                    const AnimationEntry& entry = anim.entries[entryIdx];
                    float celStartX = frameIndex * frameWidth;
                    float celWidth = entry.duration * frameWidth;
                    ImRect entryRect = getTimelineEntryRect(winPos, syncScroll, celStartX, celWidth, entryHeight);
                    ImRect handleRect = getTimelineHandleRect(entryRect, entryHeight);

                    if (handleRect.Contains(mousePos)) {
                        std::vector<int> draggedEntryIndices = getTimelineDraggedEntryIndices(
                            timelineSelectedEntryIndices,
                            entryIdx,
                            static_cast<int>(anim.entries.size()));

                        dragState.draggedEntryIdx = entryIdx;
                        dragState.draggedEntryIndices = draggedEntryIndices;
                        dragState.dragStartPosX = mousePos.x;
                        dragState.dragCurrentPosX = mousePos.x;
                        dragState.offsetFromDraggedBlockLeft =
                            mousePos.x - entryRect.Min.x +
                            getTimelineDraggedBlockOffset(anim, draggedEntryIndices, entryIdx, frameWidth);
                        dragState.targetInsertIdx = entryIdx;
                        break;
                    }

                    frameIndex += entry.duration;
                }
            }
        }
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && dragState.draggedEntryIdx >= 0) {
            float dragDelta = std::abs(io.MousePos.x - dragState.dragStartPosX);
            if (dragDelta > 5.0f) {
                dragState.isDragging = true;
                dragState.dragCurrentPosX = mousePos.x;
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            dragState.draggedEntryIdx = -1;
            dragState.draggedEntryIndices.clear();
            dragState.targetInsertIdx = -1;
            dragState.offsetFromDraggedBlockLeft = 0.0f;
        }
        return;
    }

    if (dragState.draggedEntryIdx < 0 || dragState.draggedEntryIdx >= static_cast<int>(anim.entries.size())) {
        dragState.isDragging = false;
        dragState.draggedEntryIdx = -1;
        dragState.draggedEntryIndices.clear();
        dragState.targetInsertIdx = -1;
        dragState.offsetFromDraggedBlockLeft = 0.0f;
        return;
    }

    std::vector<int> draggedEntryIndices = dragState.draggedEntryIndices.empty()
        ? getTimelineDraggedEntryIndices(timelineSelectedEntryIndices, dragState.draggedEntryIdx, static_cast<int>(anim.entries.size()))
        : dragState.draggedEntryIndices;
    if (draggedEntryIndices.empty()) {
        dragState.isDragging = false;
        dragState.draggedEntryIdx = -1;
        dragState.draggedEntryIndices.clear();
        dragState.targetInsertIdx = -1;
        dragState.offsetFromDraggedBlockLeft = 0.0f;
        return;
    }

    dragState.dragCurrentPosX = mousePos.x;
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

    float edgePadding = getScaledSize(56.0f);
    float scrollStep = getScaledSize(18.0f);
    float visibleLeft = winPos.x;
    float visibleRight = winPos.x + ImGui::GetWindowWidth();
    float nextScroll = syncScroll;

    if (mousePos.x > visibleRight - edgePadding) {
        nextScroll = ImMin(ImGui::GetScrollMaxX(), syncScroll + scrollStep);
    }
    else if (mousePos.x < visibleLeft + edgePadding) {
        nextScroll = ImMax(0.0f, syncScroll - scrollStep);
    }

    if (nextScroll != syncScroll) {
        ImGui::SetScrollX(nextScroll);
        syncScroll = nextScroll;
    }

    float draggedWidth = getTimelineDraggedWidth(anim, draggedEntryIndices, frameWidth);
    float draggedLeft = dragState.dragCurrentPosX - dragState.offsetFromDraggedBlockLeft;
    float probeFramePos = (draggedLeft - winPos.x + syncScroll + draggedWidth * 0.5f) / frameWidth;
    dragState.targetInsertIdx = getTimelineRawInsertIdx(anim, probeFramePos);

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        int finalInsertIdx = getTimelinePreviewInsertIdx(
            draggedEntryIndices,
            dragState.targetInsertIdx,
            static_cast<int>(anim.entries.size()));
        std::vector<int> previewOrder = buildTimelinePreviewOrder(
            static_cast<int>(anim.entries.size()),
            draggedEntryIndices,
            finalInsertIdx);

        if (finalInsertIdx >= 0 && !isTimelinePreviewOrderOriginal(previewOrder)) {
            auto oldEntries = anim.entries;
            std::vector<AnimationEntry> movedEntries;
            std::vector<AnimationEntry> remainingEntries;
            movedEntries.reserve(draggedEntryIndices.size());
            remainingEntries.reserve(anim.entries.size() - draggedEntryIndices.size());

            for (int idx = 0; idx < static_cast<int>(anim.entries.size()); ++idx) {
                if (isTimelineEntryIndexInList(draggedEntryIndices, idx)) {
                    movedEntries.push_back(anim.entries[idx]);
                }
                else {
                    remainingEntries.push_back(anim.entries[idx]);
                }
            }

            remainingEntries.insert(
                remainingEntries.begin() + finalInsertIdx,
                movedEntries.begin(),
                movedEntries.end());
            anim.entries = remainingEntries;

            timelineSelectedEntryIndices.clear();
            for (int i = 0; i < static_cast<int>(draggedEntryIndices.size()); ++i) {
                timelineSelectedEntryIndices.push_back(finalInsertIdx + i);
            }

            int animIdx = currentAnimation;
            undoManager.execute(std::make_unique<AnimationEntriesAction>(
                draggedEntryIndices.size() > 1 ? "Move Timeline Entries" : "Move Timeline Entry",
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

        dragState.isDragging = false;
        dragState.draggedEntryIdx = -1;
        dragState.draggedEntryIndices.clear();
        dragState.targetInsertIdx = -1;
        dragState.offsetFromDraggedBlockLeft = 0.0f;
    }
}
