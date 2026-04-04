#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"

void Sofanthiel::handleSpritesheet()
{
    ImGui::Begin("Spritesheet", nullptr, ImGuiWindowFlags_NoCollapse);
    bool spritesheetWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !spritesheetWindowHovered) {
        ssIsSelecting = false;
        ssHasSelection = false;
    }

    float infoBarHeight = ImGui::GetFrameHeightWithSpacing() * 3 + ImGui::GetStyle().ItemSpacing.y * 2;
    float contentHeight = ImMax(ImGui::GetContentRegionAvail().y - infoBarHeight, 50.0f);

    ImGui::BeginChild("SpritesheetContent", ImVec2(0, contentHeight), ImGuiChildFlags_None);

    ImVec2 contentCenter = calculateContentCenter();
    const int tilesPerRow = SDL_max(1, spritesheetTilesPerRow);
    ImVec2 baseSize = ImVec2(
        static_cast<float>(tiles.getWidth(tilesPerRow)),
        static_cast<float>(tiles.getHeight(tilesPerRow))
    );
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

    ImGui::BeginChild("SpritesheetInfo", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);

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
    const int tilesPerRow = SDL_max(1, spritesheetTilesPerRow);

    ImVec2 scaledSize = ImVec2(
        tiles.getWidth(tilesPerRow) * spritesheetView.zoom,
        tiles.getHeight(tilesPerRow) * spritesheetView.zoom
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

    const int tilesPerRow = SDL_max(1, spritesheetTilesPerRow);
    for (int i = 0; i < tiles.getSize(); i++) {
        float tileSize = 8.0f * spritesheetView.zoom;

        int tileX = i % tilesPerRow;
        int tileY = i / tilesPerRow;

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
    const int tilesPerRow = SDL_max(1, spritesheetTilesPerRow);

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
    ImGui::Text("Tiles/Row:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(getScaledSize(80));
    if (ImGui::InputInt("##SpritesheetTilesPerRow", &spritesheetTilesPerRow, 1, 8)) {
        spritesheetTilesPerRow = SDL_clamp(spritesheetTilesPerRow, 1, 256);
        if (ssHasSelection || ssIsSelecting) {
            ssHasSelection = false;
            ssIsSelecting = false;
        }
    }
    ImGui::SameLine();
    ImGui::Text("Zoom: %.0f%%", view.zoom * 100);

    float resetWidth = ImGui::CalcTextSize(ICON_FA_ROTATE " Reset").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - resetWidth - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button(ICON_FA_ROTATE " Reset")) {
        view.resetView();
    }

    ImGui::Spacing();
    ImGui::Text("Rows: %d", (tiles.getSize() + tilesPerRow - 1) / tilesPerRow);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Row")) {
        Tiles oldTiles = tiles;
        int oldSize = tiles.getSize();
        tiles.ensureSize(oldSize + tilesPerRow);
        Tiles newTiles = tiles;

        undoManager.execute(std::make_unique<LambdaAction>(
            "Add Spritesheet Row",
            [this, newTiles]() {
                this->tiles = newTiles;
            },
            [this, oldTiles]() {
                this->tiles = oldTiles;
            }
        ));
    }

    ImGui::SameLine();
    bool canRemoveRow = tiles.getSize() > 0;
    if (!canRemoveRow) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_MINUS " Row")) {
        Tiles oldTiles = tiles;
        int oldSize = tiles.getSize();
        int newSize = SDL_max(0, oldSize - tilesPerRow);
        tiles.resize(newSize);
        Tiles newTiles = tiles;

        undoManager.execute(std::make_unique<LambdaAction>(
            "Remove Spritesheet Row",
            [this, newTiles]() {
                this->tiles = newTiles;
            },
            [this, oldTiles]() {
                this->tiles = oldTiles;
            }
        ));
    }
    if (!canRemoveRow) ImGui::EndDisabled();

    int cursorX = static_cast<int>(floor(mousePosInWindow.x / view.zoom));
    int cursorY = static_cast<int>(floor(mousePosInWindow.y / view.zoom));
    ImGui::Text("Cursor: (%.0f, %.0f)  Offset: (%.0f, %.0f)",
        static_cast<float>(SDL_clamp(cursorX, 0, static_cast<int>(contentSize.x))),
        static_cast<float>(SDL_clamp(cursorY, 0, static_cast<int>(contentSize.y))),
        view.offset.x, view.offset.y);

    int tileX = static_cast<int>(floor(mousePosInWindow.x / view.zoom / 8.0f));
    int tileY = static_cast<int>(floor(mousePosInWindow.y / view.zoom / 8.0f));
    int rows = (tiles.getSize() + tilesPerRow - 1) / tilesPerRow;
    tileX = SDL_clamp(tileX, 0, tilesPerRow - 1);
    tileY = SDL_clamp(tileY, 0, SDL_max(0, rows - 1));

    int tileIndex = tileY * tilesPerRow + tileX;
    if (tileIndex >= 0 && tileIndex < tiles.getSize()) {
        int byteOffset = tileIndex * 32;
        char tileInfo[128];
        snprintf(tileInfo, sizeof(tileInfo), " Tile: %d (0x%X)  4bpp offset: 0x%X", tileIndex, tileIndex, byteOffset);
        ImGui::SameLine();
        ImGui::TextUnformatted(tileInfo);
    }
}

void Sofanthiel::handleSpritesheetSelection(const ImVec2& origin)
{
    if (tiles.getSize() <= 0) return;

    float tileSize = 8.0f * spritesheetView.zoom;
    const int tilesPerRow = SDL_max(1, spritesheetTilesPerRow);
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    if (ssImportActive) {
        int tileX = static_cast<int>(floor((mousePos.x - origin.x) / tileSize));
        int tileY = static_cast<int>(floor((mousePos.y - origin.y) / tileSize));
        tileX = SDL_clamp(tileX, 0, tilesPerRow - 1);
        tileY = SDL_clamp(tileY, 0, 255);
        ssImportTilePos = ImVec2(static_cast<float>(tileX), static_cast<float>(tileY));

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ResourceManager::importImageAtPosition(
                ssImportImagePath, tiles, palettes, currentPalette,
                static_cast<int>(ssImportTilePos.x),
                static_cast<int>(ssImportTilePos.y),
                tilesPerRow);

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

    bool clickedLeft = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool hoveredWindow = ImGui::IsWindowHovered();

    if (hoveredWindow && clickedLeft && !ImGui::GetIO().KeyCtrl) {
        int tileX = static_cast<int>(floor((mousePos.x - origin.x) / tileSize));
        int tileY = static_cast<int>(floor((mousePos.y - origin.y) / tileSize));

        int maxTileY = (tiles.getSize() + tilesPerRow - 1) / tilesPerRow;
        if (tileX >= 0 && tileX < tilesPerRow && tileY >= 0 && tileY < maxTileY) {
            ssIsSelecting = true;
            ssSelStart = ImVec2(static_cast<float>(tileX), static_cast<float>(tileY));
            ssSelEnd = ssSelStart;
            ssHasSelection = false;
        }
        else {
            ssIsSelecting = false;
            ssHasSelection = false;
        }
    }

    if (ssIsSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        int tileX = static_cast<int>(floor((mousePos.x - origin.x) / tileSize));
        int tileY = static_cast<int>(floor((mousePos.y - origin.y) / tileSize));

        int maxTileY = (tiles.getSize() + tilesPerRow - 1) / tilesPerRow;
        tileX = SDL_clamp(tileX, 0, tilesPerRow - 1);
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

    bool canUseShortcuts = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ssHasSelection && !ssImportActive;
    if (!canUseShortcuts) {
        return;
    }

    if (InputManager::isPressed(InputManager::Delete)) {
        Tiles oldTiles = tiles;

        TileData emptyTile = {};
        for (int y = ssSelTileY0; y <= ssSelTileY1; ++y) {
            for (int x = ssSelTileX0; x <= ssSelTileX1; ++x) {
                int tileIndex = y * tilesPerRow + x;
                if (tileIndex >= 0 && tileIndex < tiles.getSize()) {
                    tiles.setTile(tileIndex, emptyTile);
                }
            }
        }

        Tiles newTiles = tiles;
        undoManager.execute(std::make_unique<LambdaAction>(
            "Clear Tile Selection",
            [this, newTiles]() {
                this->tiles = newTiles;
            },
            [this, oldTiles]() {
                this->tiles = oldTiles;
            }
        ));
    }

    if (InputManager::isPressed(InputManager::Copy)) {
        int copyWidth = ssSelTileX1 - ssSelTileX0 + 1;
        int copyHeight = ssSelTileY1 - ssSelTileY0 + 1;

        ssTileClipboard.clear();
        ssTileClipboard.reserve(static_cast<size_t>(copyWidth * copyHeight));

        TileData emptyTile = {};
        for (int y = 0; y < copyHeight; ++y) {
            for (int x = 0; x < copyWidth; ++x) {
                int srcX = ssSelTileX0 + x;
                int srcY = ssSelTileY0 + y;
                int tileIndex = srcY * tilesPerRow + srcX;

                if (tileIndex >= 0 && tileIndex < tiles.getSize()) {
                    ssTileClipboard.push_back(tiles.getTile(tileIndex));
                }
                else {
                    ssTileClipboard.push_back(emptyTile);
                }
            }
        }

        ssTileClipboardWidth = copyWidth;
        ssTileClipboardHeight = copyHeight;
    }

    if (InputManager::isPressed(InputManager::Paste) && !ssTileClipboard.empty() && ssTileClipboardWidth > 0 && ssTileClipboardHeight > 0) {
        Tiles oldTiles = tiles;

        int destX = ssSelTileX0;
        int destY = ssSelTileY0;

        int maxDestX = SDL_min(tilesPerRow - 1, destX + ssTileClipboardWidth - 1);
        int maxDestY = destY + ssTileClipboardHeight - 1;
        int requiredMaxIndex = maxDestY * tilesPerRow + maxDestX;
        if (requiredMaxIndex >= 0) {
            tiles.ensureSize(requiredMaxIndex + 1);
        }

        for (int y = 0; y < ssTileClipboardHeight; ++y) {
            for (int x = 0; x < ssTileClipboardWidth; ++x) {
                int dstX = destX + x;
                int dstY = destY + y;
                if (dstX < 0 || dstX >= tilesPerRow || dstY < 0) {
                    continue;
                }

                int dstIndex = dstY * tilesPerRow + dstX;
                int srcIndex = y * ssTileClipboardWidth + x;
                if (dstIndex >= 0 && dstIndex < tiles.getSize() &&
                    srcIndex >= 0 && srcIndex < static_cast<int>(ssTileClipboard.size())) {
                    tiles.setTile(dstIndex, ssTileClipboard[srcIndex]);
                }
            }
        }

        Tiles newTiles = tiles;
        undoManager.execute(std::make_unique<LambdaAction>(
            "Paste Tile Selection",
            [this, newTiles]() {
                this->tiles = newTiles;
            },
            [this, oldTiles]() {
                this->tiles = oldTiles;
            }
        ));
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
    const int tilesPerRow = SDL_max(1, spritesheetTilesPerRow);

    auto copySelectionToClipboard = [this, tilesPerRow]() {
        int copyWidth = ssSelTileX1 - ssSelTileX0 + 1;
        int copyHeight = ssSelTileY1 - ssSelTileY0 + 1;

        ssTileClipboard.clear();
        ssTileClipboard.reserve(static_cast<size_t>(copyWidth * copyHeight));

        TileData emptyTile = {};
        for (int y = 0; y < copyHeight; ++y) {
            for (int x = 0; x < copyWidth; ++x) {
                int srcX = ssSelTileX0 + x;
                int srcY = ssSelTileY0 + y;
                int tileIndex = srcY * tilesPerRow + srcX;

                if (tileIndex >= 0 && tileIndex < tiles.getSize()) {
                    ssTileClipboard.push_back(tiles.getTile(tileIndex));
                }
                else {
                    ssTileClipboard.push_back(emptyTile);
                }
            }
        }

        ssTileClipboardWidth = copyWidth;
        ssTileClipboardHeight = copyHeight;
    };

    auto clearSelectionTiles = [this, tilesPerRow]() {
        Tiles oldTiles = tiles;

        TileData emptyTile = {};
        for (int y = ssSelTileY0; y <= ssSelTileY1; ++y) {
            for (int x = ssSelTileX0; x <= ssSelTileX1; ++x) {
                int tileIndex = y * tilesPerRow + x;
                if (tileIndex >= 0 && tileIndex < tiles.getSize()) {
                    tiles.setTile(tileIndex, emptyTile);
                }
            }
        }

        Tiles newTiles = tiles;
        undoManager.execute(std::make_unique<LambdaAction>(
            "Clear Tile Selection",
            [this, newTiles]() {
                this->tiles = newTiles;
            },
            [this, oldTiles]() {
                this->tiles = oldTiles;
            }
        ));
    };

    auto pasteClipboardToSelection = [this, tilesPerRow]() {
        Tiles oldTiles = tiles;

        int destX = ssSelTileX0;
        int destY = ssSelTileY0;

        int maxDestX = SDL_min(tilesPerRow - 1, destX + ssTileClipboardWidth - 1);
        int maxDestY = destY + ssTileClipboardHeight - 1;
        int requiredMaxIndex = maxDestY * tilesPerRow + maxDestX;
        if (requiredMaxIndex >= 0) {
            tiles.ensureSize(requiredMaxIndex + 1);
        }

        for (int y = 0; y < ssTileClipboardHeight; ++y) {
            for (int x = 0; x < ssTileClipboardWidth; ++x) {
                int dstX = destX + x;
                int dstY = destY + y;
                if (dstX < 0 || dstX >= tilesPerRow || dstY < 0) {
                    continue;
                }

                int dstIndex = dstY * tilesPerRow + dstX;
                int srcIndex = y * ssTileClipboardWidth + x;
                if (dstIndex >= 0 && dstIndex < tiles.getSize() &&
                    srcIndex >= 0 && srcIndex < static_cast<int>(ssTileClipboard.size())) {
                    tiles.setTile(dstIndex, ssTileClipboard[srcIndex]);
                }
            }
        }

        Tiles newTiles = tiles;
        undoManager.execute(std::make_unique<LambdaAction>(
            "Paste Tile Selection",
            [this, newTiles]() {
                this->tiles = newTiles;
            },
            [this, oldTiles]() {
                this->tiles = oldTiles;
            }
        ));
    };

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
        bool hasClipboard = !ssTileClipboard.empty() && ssTileClipboardWidth > 0 && ssTileClipboardHeight > 0;

        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Export Selection as Image...", nullptr, false, ssHasSelection && hasTiles)) {
            nfdchar_t* outPath = nullptr;
            nfdresult_t result = NFD_SaveDialog("png,bmp", nullptr, &outPath);
            if (result == NFD_OKAY) {
                ResourceManager::exportSelectionToImage(
                    outPath, tiles, palettes, currentPalette,
                    ssSelTileX0, ssSelTileY0,
                    ssSelTileX1 - ssSelTileX0 + 1,
                    ssSelTileY1 - ssSelTileY0 + 1,
                    tilesPerRow);
                free(outPath);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !ssHasSelection) {
            ImGui::SetTooltip("Select a region first by dragging on the spritesheet");
        }

        ImGui::Separator();

        if (ImGui::MenuItem(ICON_FA_COPY " Copy", InputManager::Copy.label.c_str(), false, ssHasSelection && hasTiles)) {
            copySelectionToClipboard();
        }

        if (ImGui::MenuItem(ICON_FA_PASTE " Paste", InputManager::Paste.label.c_str(), false, ssHasSelection && hasTiles && hasClipboard)) {
            pasteClipboardToSelection();
        }

        if (ImGui::MenuItem(ICON_FA_ERASER " Delete", InputManager::Delete.label.c_str(), false, ssHasSelection && hasTiles)) {
            clearSelectionTiles();
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
