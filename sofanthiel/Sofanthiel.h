#pragma once

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "nfd.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"
#include "ResourceManager.h"

//-----------------------------------------------------------------------------

struct ViewManager {
    float zoom = 1.0f;
    ImVec2 offset = ImVec2(0.0f, 0.0f);
    float backgroundColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    bool isPanning = false;
    ImVec2 panStartPos = ImVec2(0.0f, 0.0f);
    ImVec2 startOffset = ImVec2(0.0f, 0.0f);

    void handleZoomAndPan(bool isHovered, ImVec2 origin);
    void resetView();
    ImVec2 getScaledSize(ImVec2 baseSize);
    ImVec2 calculateOrigin(ImVec2 contentCenter, ImVec2 baseSize);
};

//-----------------------------------------------------------------------------

struct TimelineResizeState {
    bool isResizing = false;
    int resizingEntryIdx = -1;
    bool resizingRight = true; // true = right edge, false = left edge
    float resizeStartPos = 0.0f;
    int originalDuration = 0;
    int frameStartOffset = 0;
};

struct TimelineDragState {
    bool isDragging = false;
    int draggedEntryIdx = -1;
    float dragStartPosX = 0.0f;
    float dragCurrentPosX = 0.0f;
    float offsetFromEntryLeft = 0.0f;
    int targetInsertIdx = -1;
};

//-----------------------------------------------------------------------------

class Sofanthiel
{
public:
    Sofanthiel();
    int run();

private:
    //-----------------------------------------------------------------------------
    // Core Application Functions
    //-----------------------------------------------------------------------------
    bool init();
    void close();
    void processEvents();
    void update();
    void render();
    void setupDockingLayout();
    void handleDPIChange();

    //-----------------------------------------------------------------------------
    // Main UI Panel Handlers
    //-----------------------------------------------------------------------------
    void handleMenuBar();
    void handleTimeline();
    void handlePreview();
    void handleSpritesheet();
    void handlePalette();
    void handleAnimCels();
    void handleAnims();

    //-----------------------------------------------------------------------------
    // Cel Editing UI Handlers
    //-----------------------------------------------------------------------------
    void handleCelInfobar();
    void handleCelPreview();
    void handleCelOAMs();
    void handleCelEditor();
    void handleCelSpritesheet();

    //-----------------------------------------------------------------------------
    // Timeline Components
    //-----------------------------------------------------------------------------
    void drawTimelineControls();
    void drawTimelineHeaders(float timelineStartX, float& syncScroll, float frameWidth);
    void drawTimelineContent(Animation& anim, float timelineStartX, float& syncScroll, float frameWidth, float requiredWidth);
    void handleTimelineScrolling(float& syncScroll);
    void drawTimelineMarker(ImDrawList* drawList, const ImVec2& winPos, float syncScroll, float frameWidth);
    void drawTimelineEntries(Animation& anim, ImDrawList* drawList, const ImVec2& winPos,
        float syncScroll, float frameWidth, TimelineResizeState& resizeState, std::vector<int>& selectedEntryIndices, float entryHeight, std::vector<AnimationEntry>& clipboardEntries);
    void drawTimelineEntryBackground(ImDrawList* drawList, const ImVec2& winPos,
        float syncScroll, int entryIdx, float celStartX, float celWidth,
        bool isSelected, std::vector<int>& selectedEntryIndices, int& hoveredEntryIdx,
        std::vector<AnimationEntry>& clipboardEntries, float entryHeight);
    void handleTimelineEntryEdges(ImDrawList* drawList, const ImVec2& winPos, float syncScroll, int entryIdx, float celStartX, float celWidth, TimelineResizeState& resizeState, int frameIndex, AnimationEntry& entry, float entryHeight);
    void drawTimelineEntryLabel(ImDrawList* drawList, const ImVec2& winPos, float syncScroll, const std::string& celName, float celStartX, float celWidth, float entryHeight);
    void drawTimelineFrameCels(ImDrawList* drawList, const ImVec2& winPos, float syncScroll, int frameStartIndex, int duration, float frameWidth, float entryHeight);
    void handleTimelineResizing(Animation& anim, TimelineResizeState& resizeState, float syncScroll, float frameWidth);
    void handleTimelineAutoScroll(float syncScroll, float frameWidth);
    void handleTimelineDragging(Animation& anim, ImDrawList* drawList, const ImVec2& winPos,
        float syncScroll, float frameWidth, TimelineDragState& dragState, float entryHeight);

    //-----------------------------------------------------------------------------
    // Preview Components
    //-----------------------------------------------------------------------------
    void drawBackgroundTexture(ImDrawList* drawList, ImVec2 origin, ImVec2 scaledSize);
    void updateAnimationPlayback();
    void drawCurrentAnimationFrame(ImDrawList* drawList, ImVec2 origin, float zoom);
    void drawPreviewContent(const ImVec2& origin);
    void drawPreviewInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin);
    void handleAnimationDragging();
    bool isMouseOverAnimation(const ImVec2& mousePos);

    //-----------------------------------------------------------------------------
    // Spritesheet Components
    //-----------------------------------------------------------------------------
    void drawSpritesheetContent(const ImVec2& origin);
    void drawSpritesheetTiles(ImDrawList* drawList, const ImVec2& origin);
    void drawSingleTile(ImDrawList* drawList, float xPos, float yPos, int tileIndex);
    void drawSpritesheetInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin);

    //-----------------------------------------------------------------------------
    // Palette Components
    //-----------------------------------------------------------------------------
    void drawPaletteContent(const ImVec2& origin);
    void drawPaletteInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin);
    void drawPaletteColors(ImDrawList* drawList, ImVec2 origin, ImVec2 scaledSize);
    void drawPaletteGrid(ImDrawList* drawList, ImVec2 origin, ImVec2 scaledSize);
    void handleColorPicker(int editPaletteIdx, int editColorIdx, float editColor[4]);
	void initializeDefaultPalettes();

    //-----------------------------------------------------------------------------
    // OAM Preview Components
    //-----------------------------------------------------------------------------
    void drawCelPreviewInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin);
    void drawCelPreviewContent(const ImVec2& origin);
    void handleOAMDragging(const ImVec2& origin, float offsetX, float offsetY);
    void drawCelSpritesheetContent(const ImVec2& origin);
    void drawCelSpritesheetTiles(ImDrawList* drawList, const ImVec2& origin);
    void handleCelSpritesheetClicks(const ImVec2& origin);

    //-----------------------------------------------------------------------------
    // OAM/Sprite Rendering
    //-----------------------------------------------------------------------------
    void renderOAM(ImDrawList* drawList, ImVec2 origin, float zoom,
        const TengokuOAM& oam, float offsetX, float offsetY, float alpha);
    void getOAMDimensions(int objShape, int objSize, int& width, int& height);
    void renderTile(ImDrawList* drawList, float xPos, float yPos, float zoom,
        int tileIdx, int tx, int ty, int paletteIndex,
        bool hFlip, bool vFlip, float alpha);

    //-----------------------------------------------------------------------------
    // Common Utility Functions
    //-----------------------------------------------------------------------------
    void drawGrid(ImDrawList* drawList, ImVec2 origin, ImVec2 size, float zoom);
    void drawBackground(ImDrawList* drawList, ImVec2 origin, ImVec2 size, float* color);
    ImVec2 calculateContentCenter();
    
    // UI Layout Helpers
    float calculateRightAlignedPosition(const char* text, float padding = 0.0f);
    float calculateRightAlignedPosition(float elementWidth, float padding = 0.0f);
    float getScaledSize(float baseSize);
    ImVec2 getScaledButtonSize(float baseWidth, float baseHeight = 0.0f);

    //-----------------------------------------------------------------------------
    // Core Application State
    //-----------------------------------------------------------------------------
    bool isRunning = true;
    bool isDockingLayoutSetup = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    //-----------------------------------------------------------------------------
    // UI State
    //-----------------------------------------------------------------------------
    bool celEditingMode = false;
    ImVec2 backgroundOffset = ImVec2(0, 0);
    int editingCelIndex = -1;
    std::vector<int> selectedOAMIndices;
    bool isOAMDragging = false;
    ImVec2 oamDragStart;
    ImVec2 oamStartOffset;
    std::vector<ImVec2> selectedOAMStartPositions;
    bool showExitConfirmation = false;

    bool showGrid = true;
    bool showSelectionBorder = true;
    bool emphasizeSelectedOAMs = true;
    int gridSize = 8;

    //-----------------------------------------------------------------------------
    // Animation Data
    //-----------------------------------------------------------------------------
    std::vector<AnimationCel> animationCels;
    std::vector<Animation> animations;
    std::vector<Palette> palettes;
    Tiles tiles;

    int currentAnimation = -1;
    int currentAnimationCel = -1;

    //-----------------------------------------------------------------------------
    // Timeline Properties
    //-----------------------------------------------------------------------------
    int currentFrame = 0;
    int totalFrames = 0;
    bool isPlaying = false;
    bool loopAnimation = true;
    float frameRate = 60.0f;
    float syncScroll = 0.0f;

    //-----------------------------------------------------------------------------
    // Preview Properties
    //-----------------------------------------------------------------------------
    SDL_Texture* backgroundTexture = nullptr;
    bool showBackgroundTexture = false;
    ImVec2 previewSize = ImVec2(GBA_WIDTH, GBA_HEIGHT);
    ViewManager previewView;
    ImVec2 previewAnimationOffset = ImVec2(0, 0);
    bool isPreviewAnimationDragging = false;
    ImVec2 previewAnimationDragStart;
    ImVec2 previewAnimationStartOffset;
    bool showOverscanArea = false;

    //-----------------------------------------------------------------------------
    // Spritesheet Properties
    //-----------------------------------------------------------------------------
    bool usePaletteBGColor = false;
    int currentPalette = 0;
    ViewManager spritesheetView;

    //-----------------------------------------------------------------------------
    // Palette Properties
    //-----------------------------------------------------------------------------
    ViewManager paletteView;

    //-----------------------------------------------------------------------------
    // Clipboard and Dialog State
    //-----------------------------------------------------------------------------
    std::vector<TengokuOAM> oamClipboard;
    std::vector<AnimationEntry> clipboardEntries;

    AnimationCel celClipboard;
    bool hasCelClipboard = false;
    std::string animationCelFilename;
    bool showNewCelPopup = false;
    char newCelNameBuffer[256] = "";

    Animation animationClipboard;
    bool hasAnimationClipboard = false;
    bool showNewAnimationPopup = false;
    char newAnimationNameBuffer[256] = "";

    bool showRenameCelPopup = false;
    int renamingCelIndex = -1;
    char renameCelNameBuffer[128] = "";

    bool showRenameAnimationPopup = false;
    int renamingAnimationIndex = -1;
    char renameAnimationNameBuffer[128] = "";

    float dpiScale = 1.0f;
};