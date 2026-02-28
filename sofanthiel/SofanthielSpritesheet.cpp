#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"

void Sofanthiel::handleSpritesheet()
{
    ImGui::Begin("Spritesheet", nullptr, ImGuiWindowFlags_NoCollapse);

    float infoBarHeight = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y;
    float contentHeight = ImMax(ImGui::GetContentRegionAvail().y - infoBarHeight, 50.0f);

    ImGui::BeginChild("SpritesheetContent", ImVec2(0, contentHeight), ImGuiChildFlags_None);

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 baseSize = ImVec2(tiles.getWidth(), tiles.getHeight());
    ImVec2 origin = spritesheetView.calculateOrigin(contentCenter, baseSize);

    drawSpritesheetContent(origin);

    handleSpritesheetSelection(origin);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawSpritesheetSelection(drawList, origin);

    if (ssImportActive) {
        drawSpritesheetImportOverlay(drawList, origin);
    }

    handleSpritesheetContextMenu(origin);

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("SpritesheetInfo", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

    ImVec2 mousePosInWindow = ImVec2(
        ImGui::GetIO().MousePos.x - origin.x,
        ImGui::GetIO().MousePos.y - origin.y
    );

    drawSpritesheetInfoPanel(spritesheetView, mousePosInWindow, baseSize, origin);

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::drawSpritesheetContent(const ImVec2& origin)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 scaledSize = ImVec2(
        tiles.getWidth() * spritesheetView.zoom,
        tiles.getHeight() * spritesheetView.zoom
    );

    drawBackground(drawList, origin, scaledSize, spritesheetView.backgroundColor);

    drawSpritesheetTiles(drawList, origin);

    drawList->AddRect(
        origin,
        ImVec2(origin.x + scaledSize.x, origin.y + scaledSize.y),
        IM_COL32(100, 100, 100, 255)
    );

    drawGrid(drawList, origin, scaledSize, spritesheetView.zoom);

    spritesheetView.handleZoomAndPan(ImGui::IsWindowHovered(), origin);
}

void Sofanthiel::drawSpritesheetTiles(ImDrawList* drawList, const ImVec2& origin)
{
    if (tiles.getSize() <= 0 || palettes.empty()) {
        ImVec2 textPos = ImGui::GetCursorScreenPos();
        textPos.x += ImGui::GetContentRegionAvail().x * 0.5f - ImGui::CalcTextSize("No tiles or palette loaded").x * 0.5f;
        textPos.y += ImGui::GetContentRegionAvail().y * 0.5f;

        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "No tiles or palette loaded");
        return;
    }

    for (int i = 0; i < tiles.getSize(); i++) {
        float tileSize = 8.0f * spritesheetView.zoom;

        int tileX = i % TILES_PER_LINE;
        int tileY = i / TILES_PER_LINE;

        float xPos = origin.x + tileX * tileSize;
        float yPos = origin.y + tileY * tileSize;

        drawSingleTile(drawList, xPos, yPos, i);
    }
}

void Sofanthiel::drawSingleTile(ImDrawList* drawList, float xPos, float yPos, int tileIndex)
{
    if (palettes.empty()) return;

    TileData tile = tiles.getTile(tileIndex);
    float pixelSize = spritesheetView.zoom;

    int safePalette = SDL_clamp(currentPalette, 0, palettes.size() - 1);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            uint8_t colorIdx = tile.data[y][x];

            if (colorIdx == 0 && !usePaletteBGColor) continue;

            SDL_Color color = palettes[safePalette].colors[colorIdx];

            drawList->AddRectFilled(
                ImVec2(xPos + x * pixelSize, yPos + y * pixelSize),
                ImVec2(xPos + (x + 1) * pixelSize, yPos + (y + 1) * pixelSize),
                IM_COL32(color.r, color.g, color.b, 255)
            );
        }
    }
}

void Sofanthiel::drawSpritesheetInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin)
{
    ImGui::ColorEdit3("##SpriteBG", view.backgroundColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Background Color");

    ImGui::SameLine();
    ImGui::Checkbox("Pal BG", &usePaletteBGColor);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use first palette color as background");
    ImGui::SameLine();
    ImGui::Text("Pal:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(getScaledSize(55));
    if (palettes.size() > 0) {
        ImGui::SliderInt("##Palette", &currentPalette, 0, static_cast<int>(this->palettes.size()) - 1);
        currentPalette = SDL_clamp(currentPalette, 0, static_cast<int>(this->palettes.size()) - 1);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), "None");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("Zoom: %.0f%%", view.zoom * 100);

    float resetWidth = ImGui::CalcTextSize(ICON_FA_ROTATE " Reset").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - resetWidth - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button(ICON_FA_ROTATE " Reset")) {
        view.resetView();
    }

    ImGui::Text("Cursor: (%.0f, %.0f)  Offset: (%.0f, %.0f)",
        SDL_clamp(floor(mousePosInWindow.x / view.zoom), 0, contentSize.x),
        SDL_clamp(floor(mousePosInWindow.y / view.zoom), 0, contentSize.y),
        view.offset.x, view.offset.y);
}

void Sofanthiel::handleSpritesheetSelection(const ImVec2& origin)
{
    if (tiles.getSize() <= 0) return;

    float tileSize = 8.0f * spritesheetView.zoom;
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (ssImportActive) {
        int tileX = static_cast<int>(floor((mousePos.x - origin.x) / tileSize));
        int tileY = static_cast<int>(floor((mousePos.y - origin.y) / tileSize));
        tileX = SDL_clamp(tileX, 0, TILES_PER_LINE - 1);
        tileY = SDL_clamp(tileY, 0, 255);
        ssImportTilePos = ImVec2(static_cast<float>(tileX), static_cast<float>(tileY));

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ResourceManager::importImageAtPosition(
                ssImportImagePath, tiles, palettes, currentPalette,
                static_cast<int>(ssImportTilePos.x),
                static_cast<int>(ssImportTilePos.y));

            if (ssImportPreviewTex) {
                SDL_DestroyTexture(ssImportPreviewTex);
                ssImportPreviewTex = nullptr;
            }
            ssImportActive = false;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
            if (ssImportPreviewTex) {
                SDL_DestroyTexture(ssImportPreviewTex);
                ssImportPreviewTex = nullptr;
            }
            ssImportActive = false;
        }
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyCtrl) {
        int tileX = static_cast<int>(floor((mousePos.x - origin.x) / tileSize));
        int tileY = static_cast<int>(floor((mousePos.y - origin.y) / tileSize));

        int maxTileY = (tiles.getSize() + TILES_PER_LINE - 1) / TILES_PER_LINE;
        if (tileX >= 0 && tileX < TILES_PER_LINE && tileY >= 0 && tileY < maxTileY) {
            ssIsSelecting = true;
            ssSelStart = ImVec2(static_cast<float>(tileX), static_cast<float>(tileY));
            ssSelEnd = ssSelStart;
            ssHasSelection = false;
        }
    }

    if (ssIsSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        int tileX = static_cast<int>(floor((mousePos.x - origin.x) / tileSize));
        int tileY = static_cast<int>(floor((mousePos.y - origin.y) / tileSize));

        int maxTileY = (tiles.getSize() + TILES_PER_LINE - 1) / TILES_PER_LINE;
        tileX = SDL_clamp(tileX, 0, TILES_PER_LINE - 1);
        tileY = SDL_clamp(tileY, 0, maxTileY - 1);
        ssSelEnd = ImVec2(static_cast<float>(tileX), static_cast<float>(tileY));
    }

    if (ssIsSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        ssIsSelecting = false;

        ssSelTileX0 = static_cast<int>(fmin(ssSelStart.x, ssSelEnd.x));
        ssSelTileY0 = static_cast<int>(fmin(ssSelStart.y, ssSelEnd.y));
        ssSelTileX1 = static_cast<int>(fmax(ssSelStart.x, ssSelEnd.x));
        ssSelTileY1 = static_cast<int>(fmax(ssSelStart.y, ssSelEnd.y));

        ssHasSelection = true;
    }

    if (ssHasSelection && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ssHasSelection = false;
    }
}

void Sofanthiel::drawSpritesheetSelection(ImDrawList* drawList, const ImVec2& origin)
{
    if (ssIsSelecting) {
        float tileSize = 8.0f * spritesheetView.zoom;
        int x0 = static_cast<int>(fmin(ssSelStart.x, ssSelEnd.x));
        int y0 = static_cast<int>(fmin(ssSelStart.y, ssSelEnd.y));
        int x1 = static_cast<int>(fmax(ssSelStart.x, ssSelEnd.x));
        int y1 = static_cast<int>(fmax(ssSelStart.y, ssSelEnd.y));

        ImVec2 p0(origin.x + x0 * tileSize, origin.y + y0 * tileSize);
        ImVec2 p1(origin.x + (x1 + 1) * tileSize, origin.y + (y1 + 1) * tileSize);

        drawList->AddRectFilled(p0, p1, IM_COL32(100, 150, 255, 40));
        drawList->AddRect(p0, p1, IM_COL32(100, 150, 255, 200), 0, 0, 2.0f);
    }
    else if (ssHasSelection) {
        float tileSize = 8.0f * spritesheetView.zoom;

        ImVec2 p0(origin.x + ssSelTileX0 * tileSize, origin.y + ssSelTileY0 * tileSize);
        ImVec2 p1(origin.x + (ssSelTileX1 + 1) * tileSize, origin.y + (ssSelTileY1 + 1) * tileSize);

        drawList->AddRectFilled(p0, p1, IM_COL32(100, 180, 255, 30));
        drawList->AddRect(p0, p1, IM_COL32(100, 180, 255, 220), 0, 0, 2.0f);

        int tw = ssSelTileX1 - ssSelTileX0 + 1;
        int th = ssSelTileY1 - ssSelTileY0 + 1;
        char sizeLabel[64];
        snprintf(sizeLabel, sizeof(sizeLabel), "%dx%d tiles (%dx%d px)", tw, th, tw * 8, th * 8);
        ImVec2 textSize = ImGui::CalcTextSize(sizeLabel);
        ImVec2 textPos(p0.x + ((p1.x - p0.x) - textSize.x) * 0.5f, p0.y - textSize.y - 3);
        if (textPos.y < origin.y) textPos.y = p1.y + 3;
        drawList->AddText(textPos, IM_COL32(200, 220, 255, 220), sizeLabel);
    }
}

void Sofanthiel::drawSpritesheetImportOverlay(ImDrawList* drawList, const ImVec2& origin)
{
    if (!ssImportActive) return;

    float tileSize = 8.0f * spritesheetView.zoom;
    int tilesW = (ssImportImageW + 7) / 8;
    int tilesH = (ssImportImageH + 7) / 8;

    int posX = static_cast<int>(ssImportTilePos.x);
    int posY = static_cast<int>(ssImportTilePos.y);

    ImVec2 p0(origin.x + posX * tileSize, origin.y + posY * tileSize);
    ImVec2 p1(origin.x + (posX + tilesW) * tileSize, origin.y + (posY + tilesH) * tileSize);

    if (ssImportPreviewTex) {
        drawList->AddImage(ssImportPreviewTex, p0, p1);
    }

    drawList->AddRectFilled(p0, p1, IM_COL32(50, 200, 50, 30));
    drawList->AddRect(p0, p1, IM_COL32(50, 220, 50, 200), 0, 0, 2.0f);

    char label[64];
    snprintf(label, sizeof(label), ICON_FA_ARROW_DOWN " Place here (%dx%d)", tilesW * 8, tilesH * 8);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos(p0.x + ((p1.x - p0.x) - textSize.x) * 0.5f, p0.y - textSize.y - 3);
    if (textPos.y < origin.y) textPos.y = p1.y + 3;
    drawList->AddRectFilled(
        ImVec2(textPos.x - 2, textPos.y - 1),
        ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 1),
        IM_COL32(30, 30, 30, 200));
    drawList->AddText(textPos, IM_COL32(100, 255, 100, 255), label);
}

void Sofanthiel::handleSpritesheetContextMenu(const ImVec2& origin)
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

        if (dragDist < 5.0f && !ssImportActive) {
            ImGui::OpenPopup("SpritesheetContextMenu");
        }
    }

    if (ImGui::BeginPopup("SpritesheetContextMenu")) {
        bool hasTiles = tiles.getSize() > 0 && !palettes.empty();

        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Export Selection as Image...", nullptr, false, ssHasSelection && hasTiles)) {
            nfdchar_t* outPath = nullptr;
            nfdresult_t result = NFD_SaveDialog("png,bmp", nullptr, &outPath);
            if (result == NFD_OKAY) {
                ResourceManager::exportSelectionToImage(
                    outPath, tiles, palettes, currentPalette,
                    ssSelTileX0, ssSelTileY0,
                    ssSelTileX1 - ssSelTileX0 + 1,
                    ssSelTileY1 - ssSelTileY0 + 1);
                free(outPath);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !ssHasSelection) {
            ImGui::SetTooltip("Select a region first by dragging on the spritesheet");
        }

        ImGui::Separator();

        if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Import Image into Spritesheet...", nullptr, false, hasTiles)) {
            nfdchar_t* outPath = nullptr;
            nfdresult_t result = NFD_OpenDialog("png,bmp,jpg", nullptr, &outPath);
            if (result == NFD_OKAY) {
                ssImportImagePath = std::string(outPath);

                SDL_Surface* previewSurf = IMG_Load(outPath);
                if (previewSurf) {
                    ssImportImageW = previewSurf->w;
                    ssImportImageH = previewSurf->h;

                    if (ssImportPreviewTex) {
                        SDL_DestroyTexture(ssImportPreviewTex);
                    }
                    ssImportPreviewTex = SDL_CreateTextureFromSurface(renderer, previewSurf);
                    if (ssImportPreviewTex) {
                        SDL_SetTextureAlphaMod(ssImportPreviewTex, 180);
                        SDL_SetTextureScaleMode(ssImportPreviewTex, SDL_SCALEMODE_NEAREST);
                    }
                    SDL_DestroySurface(previewSurf);

                    ssImportTilePos = ImVec2(0, 0);
                    ssImportActive = true;
                    ssHasSelection = false;
                }
                free(outPath);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem(ICON_FA_XMARK " Clear Selection", nullptr, false, ssHasSelection)) {
            ssHasSelection = false;
        }

        ImGui::EndPopup();
    }
}
