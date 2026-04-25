#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"

namespace {

constexpr int kPreviewMosaicSize = 2;

bool shouldRenderOAM(const TengokuOAM& oam)
{
    return !isHiddenOAM(oam) && oam.objMode != OBJ_MODE_WINDOW && oam.objMode != OBJ_MODE_PROHIBITED;
}

float getRenderAlpha(const TengokuOAM& oam, float alpha)
{
    return (oam.objMode == OBJ_MODE_BLEND) ? (alpha * 0.6f) : alpha;
}

int getMosaicSize(const TengokuOAM& oam)
{
    return oam.mosaicFlag ? kPreviewMosaicSize : 1;
}

std::vector<int> buildRenderOrder(const AnimationCel& cel)
{
    std::vector<int> indices(cel.oams.size());
    for (int i = 0; i < static_cast<int>(cel.oams.size()); ++i) {
        indices[static_cast<size_t>(i)] = i;
    }

    std::stable_sort(indices.begin(), indices.end(), [&cel](int lhs, int rhs) {
        const TengokuOAM& left = cel.oams[static_cast<size_t>(lhs)];
        const TengokuOAM& right = cel.oams[static_cast<size_t>(rhs)];
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return lhs > rhs;
    });

    return indices;
}

std::vector<int> buildHitTestOrder(const AnimationCel& cel)
{
    std::vector<int> indices(cel.oams.size());
    for (int i = 0; i < static_cast<int>(cel.oams.size()); ++i) {
        indices[static_cast<size_t>(i)] = i;
    }

    std::stable_sort(indices.begin(), indices.end(), [&cel](int lhs, int rhs) {
        const TengokuOAM& left = cel.oams[static_cast<size_t>(lhs)];
        const TengokuOAM& right = cel.oams[static_cast<size_t>(rhs)];
        if (left.priority != right.priority) {
            return left.priority < right.priority;
        }
        return lhs < rhs;
    });

    return indices;
}

const AnimationCel* findAnimationCelByName(const std::vector<AnimationCel>& cels, const std::string& celName)
{
    for (const auto& cel : cels) {
        if (cel.name == celName) {
            return &cel;
        }
    }

    return nullptr;
}

const AnimationCel* findAnimationCelForFrame(const Animation& anim, const std::vector<AnimationCel>& cels, int frame)
{
    int frameCounter = 0;
    for (const auto& entry : anim.entries) {
        if (frame >= frameCounter && frame < frameCounter + entry.duration) {
            return findAnimationCelByName(cels, entry.celName);
        }
        frameCounter += entry.duration;
    }

    return nullptr;
}

}

void Sofanthiel::handlePreview()
{
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoCollapse);

    float infoBarHeight = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y;
    float contentHeight = ImMax(ImGui::GetContentRegionAvail().y - infoBarHeight, 50.0f);

    ImGui::BeginChild("PreviewContent", ImVec2(0, contentHeight), ImGuiChildFlags_None);

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 origin = previewView.calculateOrigin(contentCenter, previewSize);

    drawPreviewContent(origin);

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("PreviewInfo", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImVec2 mousePosInWindow = ImVec2(
        ImGui::GetIO().MousePos.x - origin.x,
        ImGui::GetIO().MousePos.y - origin.y
    );

    drawPreviewInfoPanel(previewView, mousePosInWindow, previewSize, origin);

    ImGui::EndChild();
    ImGui::End();

    updateAnimationPlayback();
}

void Sofanthiel::drawPreviewContent(const ImVec2& origin)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 scaledSize = previewView.getScaledSize(previewSize);

    ImVec2 scaledSize2 = previewView.getScaledSize(ImVec2(256, 256));
    ImVec2 origin2 = previewView.calculateOrigin(calculateContentCenter(), ImVec2(256, 256));

    drawBackgroundTexture(drawList, origin, scaledSize);

    drawBackground(drawList, showOverscanArea ? origin2 : origin, showOverscanArea ? scaledSize2 : scaledSize, previewView.backgroundColor);

    if (showOverscanArea) {
        drawList->AddRect(
            origin2,
            ImVec2(origin2.x + scaledSize2.x, origin2.y + scaledSize2.y),
            IM_COL32(100, 100, 100, 255)
		);
    }
    drawList->AddRect(
        origin,
        ImVec2(origin.x + scaledSize.x, origin.y + scaledSize.y),
        IM_COL32(100, 100, 100, 255)
    );

    drawCurrentAnimationFrame(drawList, origin, previewView.zoom);

    drawGrid(drawList, showOverscanArea ? origin2 : origin , showOverscanArea ? scaledSize2 : scaledSize, previewView.zoom);

    handleAnimationDragging();

    previewView.handleZoomAndPan(ImGui::IsWindowHovered(), origin);
}

void Sofanthiel::drawPreviewInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin)
{
    ImGui::ColorEdit4("##BG", view.backgroundColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Background Color");

    ImGui::SameLine();
    if (ImGui::Checkbox("BG Image", &showBackgroundTexture)) {
        this->previewView.backgroundColor[3] = !showBackgroundTexture ? 1.0f : 0.5f;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Overscan", &showOverscanArea);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Text("Zoom: %.0f%%", view.zoom * 100);

    float resetWidth = ImGui::CalcTextSize(ICON_FA_ROTATE " Reset").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - resetWidth - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button(ICON_FA_ROTATE " Reset")) {
        view.resetView();
        previewAnimationOffset = ImVec2(0, 0);
    }

    ImGui::Text("Cursor: (%.0f, %.0f)  Offset: (%.0f, %.0f)",
        SDL_clamp(floor(mousePosInWindow.x / view.zoom), 0, contentSize.x),
        SDL_clamp(floor(mousePosInWindow.y / view.zoom), 0, contentSize.y),
        view.offset.x, view.offset.y);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("Anim Offset:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(getScaledSize(80));
    ImGui::DragFloat2("##AnimOffset", (float*)&previewAnimationOffset, 1.0f, -256.0f, 255.0f, "%.0f");
}

void Sofanthiel::drawBackgroundTexture(ImDrawList* drawList, ImVec2 origin, ImVec2 scaledSize) {
    if (backgroundTexture != nullptr && showBackgroundTexture) {
        float texWidth, texHeight;
        SDL_GetTextureSize(backgroundTexture, &texWidth, &texHeight);

        float texAspect = (float)texWidth / (float)texHeight;
        float previewAspect = previewSize.x / previewSize.y;

        float texScaleX, texScaleY;
        if (texAspect > previewAspect) {
            texScaleX = previewSize.x;
            texScaleY = texScaleX / texAspect;
        }
        else {
            texScaleY = previewSize.y;
            texScaleX = texScaleY * texAspect;
        }

        ImVec2 texPos = ImVec2(
            origin.x + (previewSize.x - texScaleX) * 0.5f * previewView.zoom,
            origin.y + (previewSize.y - texScaleY) * 0.5f * previewView.zoom
        );

        ImVec2 texSize = ImVec2(texScaleX * previewView.zoom, texScaleY * previewView.zoom);

        drawList->AddImage(
            backgroundTexture,
            texPos,
            ImVec2(texPos.x + texSize.x, texPos.y + texSize.y)
        );
    }
}

void Sofanthiel::updateAnimationPlayback() {
    static float lastTime = (float)SDL_GetTicks() / 1000.0f;
    float currentTime = (float)SDL_GetTicks() / 1000.0f;
    float deltaTime = currentTime - lastTime;

    if (isPlaying && deltaTime >= (1.0f / frameRate) && totalFrames != 0) {
        currentFrame++;
        if (currentFrame >= totalFrames) {
            if (loopAnimation) {
                currentFrame = 0;
            }
            else {
                currentFrame = totalFrames - 1;
                isPlaying = false;
            }
        }
        lastTime = currentTime;
    }
}

void Sofanthiel::drawCurrentAnimationFrame(ImDrawList* drawList, ImVec2 origin, float zoom) {
    if (currentAnimation < 0 || currentAnimation >= animations.size() ||
        animationCels.empty() || totalFrames <= 0) {
        return;
    }

    drawAnimationFramePreview(
        drawList,
        origin,
        zoom,
        animations[currentAnimation],
        animationCels,
        currentFrame,
        previewAnimationOffset);
}

void Sofanthiel::drawAnimationFramePreview(ImDrawList* drawList, ImVec2 origin, float zoom,
    const Animation& anim, const std::vector<AnimationCel>& cels,
    int frame, ImVec2 animationOffset)
{
    if (anim.entries.empty() || cels.empty()) {
        return;
    }

    const AnimationCel* cel = findAnimationCelForFrame(anim, cels, frame);
    if (cel == nullptr) {
        return;
    }

    float offsetX = previewSize.x / 2.0f + animationOffset.x;
    float offsetY = previewSize.y / 2.0f + animationOffset.y;
    bool canRenderSpritePixels = tiles.getSize() > 0 && !palettes.empty();

    const std::vector<int> renderOrder = buildRenderOrder(*cel);
    for (int index : renderOrder) {
        const TengokuOAM& oam = cel->oams[static_cast<size_t>(index)];

        if (canRenderSpritePixels) {
            renderOAM(drawList, origin, zoom, oam, offsetX, offsetY, 1.0f);
            continue;
        }

        if (!shouldRenderOAM(oam)) {
            continue;
        }

        int width = 0;
        int height = 0;
        getOAMDimensions(oam.objShape, oam.objSize, width, height);

        ImVec2 min(
            origin.x + (oam.xPosition + offsetX) * zoom,
            origin.y + (oam.yPosition + offsetY) * zoom);
        ImVec2 max(
            min.x + width * zoom,
            min.y + height * zoom);

        ImU32 boxColor = IM_COL32(
            80 + (index * 53) % 160,
            120 + (index * 37) % 100,
            180 + (index * 29) % 70,
            static_cast<unsigned char>(getRenderAlpha(oam, 1.0f) * 255.0f));

        drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, 18));
        drawList->AddRect(min, max, boxColor, 0.0f, 0, 2.0f);
    }
}

void Sofanthiel::handleAnimationDragging()
{
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isOverSprite = isMouseOverAnimation(mousePos);

    if (isOverSprite && !isPreviewAnimationDragging) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (ImGui::IsWindowHovered() || isPreviewAnimationDragging) {
        if (!isPreviewAnimationDragging && isOverSprite && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            isPreviewAnimationDragging = true;
            previewAnimationDragStart = mousePos;
            previewAnimationStartOffset = previewAnimationOffset;
        }

        if (isPreviewAnimationDragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 delta = ImVec2(
                    mousePos.x - previewAnimationDragStart.x,
                    mousePos.y - previewAnimationDragStart.y
                );

                ImVec2 newOffset = ImVec2(
                    previewAnimationStartOffset.x + delta.x / previewView.zoom,
                    previewAnimationStartOffset.y + delta.y / previewView.zoom
                );

                previewAnimationOffset = ImVec2(
                    floor(newOffset.x + 0.5f),
                    floor(newOffset.y + 0.5f)
                );

                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            else {
                isPreviewAnimationDragging = false;
            }
        }
    }
    else if (isPreviewAnimationDragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        isPreviewAnimationDragging = false;
    }

    previewAnimationOffset.x = SDL_clamp(previewAnimationOffset.x, -256.0f, 255.0f);
    previewAnimationOffset.y = SDL_clamp(previewAnimationOffset.y, -128.0f, 127.0f);
}

bool Sofanthiel::isMouseOverAnimation(const ImVec2& mousePos)
{
    if (currentAnimation < 0 || currentAnimation >= animations.size() ||
        animationCels.empty() || totalFrames <= 0) {
        return false;
    }

    const Animation& anim = animations[currentAnimation];
    int frameCounter = 0;
    std::string currentCelName;

    for (const auto& entry : anim.entries) {
        if (currentFrame >= frameCounter && currentFrame < frameCounter + entry.duration) {
            currentCelName = entry.celName;
            break;
        }
        frameCounter += entry.duration;
    }

    const AnimationCel* cel = nullptr;
    for (const auto& c : animationCels) {
        if (c.name == currentCelName) {
            cel = &c;
            break;
        }
    }

    if (cel == nullptr) return false;

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 origin = previewView.calculateOrigin(contentCenter, previewSize);

    float offsetX = previewSize.x / 2.0f + previewAnimationOffset.x;
    float offsetY = previewSize.y / 2.0f + previewAnimationOffset.y;

    const std::vector<int> hitTestOrder = buildHitTestOrder(*cel);
    for (int index : hitTestOrder) {
        const TengokuOAM& oam = cel->oams[static_cast<size_t>(index)];
        if (!shouldRenderOAM(oam)) {
            continue;
        }

        int width = 0, height = 0;
        getOAMDimensions(oam.objShape, oam.objSize, width, height);

        float xPos = origin.x + (oam.xPosition + offsetX) * previewView.zoom;
        float yPos = origin.y + (oam.yPosition + offsetY) * previewView.zoom;

        ImVec2 spriteMin = ImVec2(xPos, yPos);
        ImVec2 spriteMax = ImVec2(xPos + width * previewView.zoom, yPos + height * previewView.zoom);

        if (mousePos.x >= spriteMin.x && mousePos.x < spriteMax.x &&
            mousePos.y >= spriteMin.y && mousePos.y < spriteMax.y) {

            int localX = (int)((mousePos.x - xPos) / previewView.zoom);
            int localY = (int)((mousePos.y - yPos) / previewView.zoom);

            int paletteIndex = oam.palette;
            if (paletteIndex >= static_cast<int>(palettes.size())) continue;

            int tileX = localX / 8;
            int tileY = localY / 8;

            if (oam.hFlip) tileX = (width / 8) - 1 - tileX;
            if (oam.vFlip) tileY = (height / 8) - 1 - tileY;

            int tileIdx = getTileIndexForOffset(oam, tileX, tileY);
            if (tileIdx >= tiles.getSize()) continue;

            int pixelX = localX % 8;
            int pixelY = localY % 8;

            const int mosaicSize = getMosaicSize(oam);
            pixelX = (pixelX / mosaicSize) * mosaicSize;
            pixelY = (pixelY / mosaicSize) * mosaicSize;

            if (oam.hFlip) pixelX = 7 - pixelX;
            if (oam.vFlip) pixelY = 7 - pixelY;

            TileData tile = tiles.getTile(tileIdx);
            uint8_t colorIdx = tile.data[pixelY][pixelX];
            SDL_Color color = {};
            if (getOAMColor(palettes, oam, colorIdx, color)) return true;
        }
    }

    return false;
}

void Sofanthiel::renderOAM(ImDrawList* drawList, ImVec2 origin, float zoom,
    const TengokuOAM& oam, float offsetX, float offsetY, float alpha) {
    if (!shouldRenderOAM(oam)) {
        return;
    }

    int width = 0, height = 0;
    getOAMDimensions(oam.objShape, oam.objSize, width, height);

    float xPos = origin.x + (oam.xPosition + offsetX) * zoom;
    float yPos = origin.y + (oam.yPosition + offsetY) * zoom;

    const float renderAlpha = getRenderAlpha(oam, alpha);

    for (int ty = 0; ty < height / 8; ty++) {
        for (int tx = 0; tx < width / 8; tx++) {
            int tileX = oam.hFlip ? (width / 8 - 1 - tx) : tx;
            int tileY = oam.vFlip ? (height / 8 - 1 - ty) : ty;
            int tileIdx = getTileIndexForOffset(oam, tileX, tileY);

            if (tileIdx >= tiles.getSize()) continue;

            renderTile(drawList, xPos, yPos, zoom, oam, tileIdx, tx, ty, renderAlpha);
        }
    }
}

void Sofanthiel::renderTile(ImDrawList* drawList, float xPos, float yPos, float zoom,
    const TengokuOAM& oam, int tileIdx, int tx, int ty, float alpha) {
    TileData tile = tiles.getTile(tileIdx);
    const int mosaicSize = getMosaicSize(oam);

    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int sampleX = (px / mosaicSize) * mosaicSize;
            int sampleY = (py / mosaicSize) * mosaicSize;
            int pixelX = oam.hFlip ? (7 - sampleX) : sampleX;
            int pixelY = oam.vFlip ? (7 - sampleY) : sampleY;

            uint8_t colorIdx = tile.data[pixelY][pixelX];
            SDL_Color color = {};
            if (!getOAMColor(palettes, oam, colorIdx, color)) continue;

            float finalX = xPos + (tx * 8 + px) * zoom;
            float finalY = yPos + (ty * 8 + py) * zoom;

            drawList->AddRectFilled(
                ImVec2(finalX, finalY),
                ImVec2(finalX + zoom, finalY + zoom),
                IM_COL32(color.r, color.g, color.b, (unsigned char)(alpha * 255))
            );
        }
    }
}

void Sofanthiel::getOAMDimensions(int objShape, int objSize, int& width, int& height) {
    switch (objShape) {
    case SHAPE_SQUARE:
        switch (objSize) {
        case 0: width = 8; height = 8; break;
        case 1: width = 16; height = 16; break;
        case 2: width = 32; height = 32; break;
        case 3: width = 64; height = 64; break;
        }
        break;
    case SHAPE_HORIZONTAL:
        switch (objSize) {
        case 0: width = 16; height = 8; break;
        case 1: width = 32; height = 8; break;
        case 2: width = 32; height = 16; break;
        case 3: width = 64; height = 32; break;
        }
        break;
    case SHAPE_VERTICAL:
        switch (objSize) {
        case 0: width = 8; height = 16; break;
        case 1: width = 8; height = 32; break;
        case 2: width = 16; height = 32; break;
        case 3: width = 32; height = 64; break;
        }
        break;
    }
}
