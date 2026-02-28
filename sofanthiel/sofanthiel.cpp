#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"

Sofanthiel::Sofanthiel()
{
    this->animationCelFilename = "placeholder_anim.inc.c";

    this->initializeDefaultPalettes();
}

int Sofanthiel::run()
{
    if(!this->init()){
        SDL_Log("Failed to initialize Sofanthiel");
        this->close();
        return 1;
	}
	SDL_Log("Initialized Sofanthiel successfully~!");

    while (this->isRunning) {
        this->processEvents();
        this->update();
        this->render();
    }

    this->close();
    return 0;
}

bool Sofanthiel::init()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

    this->window = SDL_CreateWindow("Sofanthiel (v1.60)", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (this->window == nullptr)
    {
        SDL_Log("Error creating window: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowIcon(this->window, IMG_Load("assets/icon.png"));

    // Get DPI scale factor
    float displayScale = SDL_GetWindowDisplayScale(this->window);
    if (displayScale <= 0.0f) {
        displayScale = 1.0f; // Fallback to 1.0 if unable to get scale
    }

    SDL_Log("Display scale factor: %.2f", displayScale);

	this->renderer = SDL_CreateRenderer(this->window, NULL);
    if (this->renderer == nullptr) {
        SDL_Log("Error creating renderer: %s\n", SDL_GetError());
		return false;
    }
	SDL_SetRenderVSync(this->renderer, true);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    int windowWidth, windowHeight;
    SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);
    io.DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);

    // Store display scale for future use
    this->dpiScale = displayScale;

    // Configure ImGui style for DPI scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(displayScale);

	ImGui_ImplSDL3_InitForSDLRenderer(this->window, this->renderer);
    if(!ImGui_ImplSDLRenderer3_Init(this->renderer)) {
        SDL_Log("Error initializing ImGui renderer: %s\n", SDL_GetError());
        return false;
	}

    // Load fonts with proper DPI scaling
    float baseFontSize = 13.0f * displayScale;
    float iconFontSize = baseFontSize * 2.0f / 3.0f;

    io.Fonts->Clear();

    // Load default font with proper size
    ImFontConfig defaultConfig;
    defaultConfig.SizePixels = baseFontSize;
    defaultConfig.OversampleH = 2;
    defaultConfig.OversampleV = 1;
    defaultConfig.PixelSnapH = true;
    io.Fonts->AddFontDefault(&defaultConfig);

    // Merge in icons from Font Awesome with proper scaling
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 2;
    icons_config.OversampleV = 1;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    
    // Try to load FontAwesome icons, but don't fail if the file is missing
    ImFont* iconFont = io.Fonts->AddFontFromFileTTF("assets/" FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges);
    if (iconFont == nullptr) {
        SDL_Log("Warning: Could not load FontAwesome icons from assets/" FONT_ICON_FILE_NAME_FAS);
    }

    io.Fonts->Build();

    return true;
}

void Sofanthiel::close()
{
    SDL_Log("bye bye!");

    // Destroy background texture
    if (this->backgroundTexture != nullptr) {
        SDL_DestroyTexture(this->backgroundTexture);
        this->backgroundTexture = nullptr;
    }

	// Cleanup ImGui
    if(ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
	}

    // Cleanup SDL Renderer
    if (this->renderer != nullptr) {
        SDL_DestroyRenderer(this->renderer);
        this->renderer = nullptr;
	}

    // Cleanup SDL Window
    if (this->window != nullptr) {
        SDL_DestroyWindow(this->window);
        this->window = nullptr;
    }

    // Quit SDL
	SDL_Quit();
}

void Sofanthiel::processEvents()
{
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        
        if(event.type == SDL_EVENT_QUIT) {
            showExitConfirmation = true;
        }
        else if(event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
            handleDPIChange();
        }
	}

    if (ImGui::IsKeyPressed(ImGuiKey_Space) && !ImGui::GetIO().WantCaptureKeyboard && !this->celEditingMode) {
        isPlaying = !isPlaying;
    }
}

void Sofanthiel::update()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(ImGui::GetID("MyDockSpace"), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    
    if (!isDockingLayoutSetup) {
        this->setupDockingLayout();
        isDockingLayoutSetup = true;
    }

    handleMenuBar();

    if (showExitConfirmation) {
        ImGui::OpenPopup("Confirm Exit");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Confirm Exit", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to exit Sofanthiel?");
            ImGui::Separator();

            if (ImGui::Button("Yes", getScaledButtonSize(120, 0))) {
                this->isRunning = false;
                ImGui::CloseCurrentPopup();
                showExitConfirmation = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("No", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
                showExitConfirmation = false;
            }

            ImGui::EndPopup();
        }
    }

    if (!this->celEditingMode) {
        handleTimeline();
        handlePreview();
        handleSpritesheet();
        handlePalette();
        handleAnimCels();
        handleAnims();
    }
    else {
        handleCelInfobar();
        handleCelPreview();
        handleCelOAMs();
        handleCelEditor();
        handleCelSpritesheet();
    }
}

void Sofanthiel::render()
{
    ImGui::Render();
    SDL_SetRenderDrawColor(this->renderer, 45, 45, 45, 255);
    SDL_RenderClear(this->renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), this->renderer);
    SDL_RenderPresent(this->renderer);
}

void Sofanthiel::setupDockingLayout()
{
    ImGuiID dockspaceID = ImGui::GetID("MyDockSpace");
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID);
    ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetIO().DisplaySize);

    ImGuiID dockIDMain = dockspaceID;
    ImGuiID dockIDBottom = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Down, 0.2f, nullptr, &dockIDMain);
    ImGuiID dockIDTop = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Up, 0.15f, nullptr, &dockIDMain);
    ImGuiID dockIDLeft = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Left, 0.2f, nullptr, &dockIDMain);
    ImGuiID dockIDRight = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Right, 0.2f, nullptr, &dockIDMain);
    
	// Main windows
    ImGui::DockBuilderDockWindow("Preview", dockIDMain);
	ImGui::DockBuilderDockWindow("Spritesheet", dockIDMain);
	ImGui::DockBuilderDockWindow("Palette", dockIDMain);
    ImGui::DockBuilderDockWindow("Timeline", dockIDBottom);
    ImGui::DockBuilderDockWindow("Toolbar", dockIDLeft);
	ImGui::DockBuilderDockWindow("Animation Cels", dockIDLeft);
	ImGui::DockBuilderDockWindow("Animations", dockIDTop);

	// Cel Editor windows
	ImGui::DockBuilderDockWindow("Cel Preview", dockIDMain);
	ImGui::DockBuilderDockWindow("OAMs", dockIDLeft);
    ImGui::DockBuilderDockWindow("Cel Info", dockIDTop);
    ImGui::DockBuilderDockWindow("Cel Editor", dockIDRight);
    ImGui::DockBuilderDockWindow("Spritesheet##", dockIDBottom);

    ImGui::DockBuilderFinish(dockspaceID);
}

void Sofanthiel::handleMenuBar()
{
    nfdchar_t* outPath = nullptr;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_FA_FILE " New")) {
                this->animationCels.clear();
                this->animations.clear();
                this->palettes.clear();
                this->tiles = Tiles();
                this->celEditingMode = false;
                this->editingCelIndex = -1;
				this->selectedOAMIndices.clear();

                this->initializeDefaultPalettes();
                this->currentPalette = 0;
            }
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open", 0, nullptr, false)) {
            }
            if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Save", 0, nullptr, false)) {
            }
            ImGui::Separator();
            if (ImGui::BeginMenu(ICON_FA_FILE_IMPORT " Import")) {
                if (ImGui::MenuItem(ICON_FA_IMAGE " Spritesheet (.4bpp, .bin, .image)")) {
					nfdresult_t result = NFD_OpenDialog("4bpp,bin,png,bmp", nullptr, &outPath);

                    if (result == NFD_OKAY) {
						std::string outPathStr(outPath);
                        if (outPathStr.substr(outPathStr.find_last_of(".") + 1) == "4bpp" || outPathStr.substr(outPathStr.find_last_of(".") + 1) == "bin") {
                            this->tiles = ResourceManager::loadTiles(outPath);
                        } else {
                            this->tiles = ResourceManager::loadTilesFromImageAndPalette(outPath, this->palettes, this->currentPalette);
						}
                        free(outPath);
                    }
                }
                if (ImGui::MenuItem(ICON_FA_PALETTE " Palette (.pal)")) {
                    nfdresult_t result = NFD_OpenDialog("pal", nullptr, &outPath);

                    if (result == NFD_OKAY) {
                        this->palettes = ResourceManager::loadPalettes(outPath);
                        free(outPath);
                    }
                }
                if (ImGui::MenuItem(ICON_FA_LAYER_GROUP " Animation Cels (.inc.c)")) {
                    nfdresult_t result = NFD_OpenDialog("c", nullptr, &outPath);

                    if (result == NFD_OKAY) {
                        this->animationCels = ResourceManager::loadAnimationCels(outPath);
                        std::string fullPath(outPath);
                        size_t lastSlash = fullPath.find_last_of("/\\");
                        if (lastSlash != std::string::npos)
                            this->animationCelFilename = fullPath.substr(lastSlash + 1);
                        else
                            this->animationCelFilename = fullPath;
                        free(outPath);
                    }
                }
                if (ImGui::MenuItem(ICON_FA_CLAPPERBOARD " Animations (.c)")) {
                    nfdresult_t result = NFD_OpenDialog("c", nullptr, &outPath);

                    if (result == NFD_OKAY) {
                        this->animations = ResourceManager::loadAnimations(outPath);
                        free(outPath);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_FILE_EXPORT " Export")) {
                if (ImGui::MenuItem(ICON_FA_LAYER_GROUP " Animation Cels (.inc.c)")) {
                    nfdresult_t result = NFD_SaveDialog("c", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        ResourceManager::saveAnimationCels(outPath, this->animationCels);
                        free(outPath);
					}
                }
                if (ImGui::MenuItem(ICON_FA_CLAPPERBOARD " Animations (.c)")) {
                    nfdresult_t result = NFD_SaveDialog("c", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        ResourceManager::saveAnimations(outPath, this->animations, this->animationCelFilename);
                        free(outPath);
                    }
                }
                if (ImGui::MenuItem(ICON_FA_IMAGE " Spritesheet (.4bpp, .bin, .image)")) {
                    nfdresult_t result = NFD_SaveDialog("4bpp,bin,png,bmp", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        std::string outPathStr(outPath);
                        if (outPathStr.substr(outPathStr.find_last_of(".") + 1) == "4bpp" || outPathStr.substr(outPathStr.find_last_of(".") + 1) == "bin") {
                            ResourceManager::saveTiles(outPath, this->tiles);
                        }
                        else {
                            ResourceManager::saveTilesToImage(outPath, this->tiles, this->palettes);
                        }
                        free(outPath);
                    }
				}
                if (ImGui::MenuItem(ICON_FA_PALETTE " Palettes (.pal, .c)")) {
					nfdresult_t result = NFD_SaveDialog("pal,c", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        ResourceManager::savePalettes(outPath, this->palettes);
                        free(outPath);
                    }
				}
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_DOOR_OPEN " Exit")) {
                showExitConfirmation = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            bool inCelEditor = this->celEditingMode;
            bool hasSelection = inCelEditor && editingCelIndex >= 0 &&
                editingCelIndex < animationCels.size() &&
                !selectedOAMIndices.empty();
            bool hasClipboard = !oamClipboard.empty();

            if (ImGui::MenuItem(ICON_FA_COPY " Copy", "Ctrl+C", false, hasSelection)) {
                if (inCelEditor) {
                    AnimationCel& cel = animationCels[editingCelIndex];
                    oamClipboard.clear();
                    for (int index : selectedOAMIndices) {
                        if (index >= 0 && index < cel.oams.size()) {
                            oamClipboard.push_back(cel.oams[index]);
                        }
                    }
                }
            }

            if (ImGui::MenuItem(ICON_FA_PASTE " Paste", "Ctrl+V", false, hasSelection && hasClipboard)) {
                if (inCelEditor) {
                    AnimationCel& cel = animationCels[editingCelIndex];
                    int insertPos = selectedOAMIndices.back() + 1;

                    cel.oams.insert(cel.oams.begin() + insertPos,
                        oamClipboard.begin(),
                        oamClipboard.end());

                    selectedOAMIndices.clear();
                    for (size_t i = 0; i < oamClipboard.size(); i++) {
                        selectedOAMIndices.push_back(insertPos + i);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem(ICON_FA_ROTATE " Reset Layout")) {
                isDockingLayoutSetup = false;
            }
            if (ImGui::BeginMenu(ICON_FA_TABLE_CELLS " Grid Settings")) {
                ImGui::MenuItem("Show Grid", nullptr, &showGrid);
                if (ImGui::BeginMenu("Grid Size")) {
                    for (int i = 0; i < 5; i++) {
                        int size = (i == 0) ? 1 : std::pow(2, i);
                        if (ImGui::MenuItem((std::to_string(size) + "x" + std::to_string(size)).c_str(), nullptr, gridSize == size)) {
                            gridSize = size;
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(ICON_FA_IMAGE " Load Background")) {
                nfdresult_t result = NFD_OpenDialog("png,jpg,bmp", nullptr, &outPath);

                if (result == NFD_OKAY) {
                    SDL_Texture* bgTex = ResourceManager::loadTexture(this->renderer, outPath);
                    if( bgTex != nullptr) {
                        if (this->backgroundTexture != nullptr) {
                            SDL_DestroyTexture(this->backgroundTexture);
                        }
                        this->backgroundTexture = bgTex;
                        SDL_SetTextureScaleMode(this->backgroundTexture, SDL_SCALEMODE_NEAREST);
                    } else {
                        SDL_Log("Failed to load background texture: %s", SDL_GetError());
					}
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (this->celEditingMode && editingCelIndex >= 0 && editingCelIndex < animationCels.size()) {
        bool hasSelection = !selectedOAMIndices.empty();

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && hasSelection) {
            AnimationCel& cel = animationCels[editingCelIndex];
            oamClipboard.clear();
            for (int index : selectedOAMIndices) {
                if (index >= 0 && index < cel.oams.size()) {
                    oamClipboard.push_back(cel.oams[index]);
                }
            }
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) &&
            !oamClipboard.empty() && selectedOAMIndices.size() > 0) {

            AnimationCel& cel = animationCels[editingCelIndex];
            int insertPos = selectedOAMIndices.back() + 1;

            cel.oams.insert(cel.oams.begin() + insertPos,
                oamClipboard.begin(),
                oamClipboard.end());

            selectedOAMIndices.clear();
            for (size_t i = 0; i < oamClipboard.size(); i++) {
                selectedOAMIndices.push_back(insertPos + i);
            }
        }
    }
}

void Sofanthiel::handleTimeline()
{
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse);

    if (currentAnimation < 0 || currentAnimation >= animations.size()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 textSize = ImGui::CalcTextSize("No animation selected");
        ImGui::SetCursorPosX((avail.x - textSize.x) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No animation selected");
        ImGui::End();
        return;
    }

    Animation& anim = animations[currentAnimation];

    totalFrames = 0;
    for (const AnimationEntry& cel : anim.entries) {
        totalFrames += cel.duration;
    }

    drawTimelineControls();
    ImGui::Separator();

    float frameWidth = getScaledSize(15.0f);
    float timelineStartX = getScaledSize(60.0f);
    float requiredWidth = timelineStartX + frameWidth * totalFrames;

    drawTimelineHeaders(timelineStartX, syncScroll, frameWidth);
    drawTimelineContent(anim, timelineStartX, syncScroll, frameWidth, requiredWidth);

    ImGui::End();
}
void Sofanthiel::handleCelInfobar() {
    ImGui::Begin("Cel Info", nullptr, ImGuiWindowFlags_NoCollapse);

    // button back
    if (ImGui::Button("Back")) {
        this->celEditingMode = false;
    }

    ImGui::SameLine();
	ImGui::Text("%s", editingCelIndex >= 0 && editingCelIndex < animationCels.size() ? animationCels[editingCelIndex].name.c_str() : "No Cel Selected");
	// vertical separator
    ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("Preview Settings:");
    ImGui::SameLine();
    ImGui::Checkbox("Show Selection Border", &showSelectionBorder);
    ImGui::SameLine();
    ImGui::Checkbox("Emphasize Selected OAMs", &emphasizeSelectedOAMs);

    ImGui::End();
}
void Sofanthiel::handleCelPreview() {
    ImGui::Begin("Cel Preview", nullptr, ImGuiWindowFlags_NoCollapse);

    if (editingCelIndex < 0 || editingCelIndex >= animationCels.size()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No cel selected for editing");
        ImGui::End();
        return;
    }

    float infoHeight = ImGui::GetTextLineHeightWithSpacing() * 2.5 + ImGui::GetStyle().ItemSpacing.y * 2;
    float contentHeight = ImGui::GetContentRegionAvail().y - infoHeight - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("CelPreviewContent", ImVec2(0, contentHeight));

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
        }
    }

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("CelPreviewInfo");

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

    // Apply background offset to the origin
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

    // Render OAMs in reverse order so that earlier OAMs are drawn on top (proper z-ordering)
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
    ImGui::ColorEdit4("Background Color", view.backgroundColor, ImGuiColorEditFlags_NoInputs);

    ImGui::SameLine(ImGui::GetWindowWidth() - (((showBackgroundTexture && backgroundTexture != nullptr)) ? 390 : 234));
    if (ImGui::Checkbox("Show Background", &showBackgroundTexture)) {
        this->previewView.backgroundColor[3] = !showBackgroundTexture ? 1.0f : 0.5f;
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

    if (showBackgroundTexture && backgroundTexture != nullptr) {
        ImGui::SameLine();
        ImGui::Text("BG Offset:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(getScaledSize(70));
        ImGui::DragFloat2("##BGOffset", (float*)&backgroundOffset, 1.0f, -500.0f, 500.0f, "%.0f");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        view.resetView();
    }

    ImGui::Text("Zoom: %.0f%% - Cursor: (%.0f, %.0f)",
        view.zoom * 100,
        SDL_clamp(floor(mousePosInWindow.x / view.zoom) - 128, -128, 127),
        SDL_clamp(floor(mousePosInWindow.y / view.zoom) - 128, -128, 127));

    ImGui::SameLine(calculateRightAlignedPosition("Show Grid", getScaledSize(10)));
    ImGui::Checkbox("Show Grid", &showGrid);
}

void Sofanthiel::renderOAM(ImDrawList* drawList, ImVec2 origin, float zoom,
    const TengokuOAM& oam, float offsetX, float offsetY, float alpha) {
    int width = 0, height = 0;
    getOAMDimensions(oam.objShape, oam.objSize, width, height);

    float xPos = origin.x + (oam.xPosition + offsetX) * zoom;
    float yPos = origin.y + (oam.yPosition + offsetY) * zoom;

    int paletteIndex = oam.palette;
    if (paletteIndex >= palettes.size() || palettes.empty()) return;

    for (int ty = 0; ty < height / 8; ty++) {
        for (int tx = 0; tx < width / 8; tx++) {
            int tileX = oam.hFlip ? (width / 8 - 1 - tx) : tx;
            int tileY = oam.vFlip ? (height / 8 - 1 - ty) : ty;
            int tileIdx = oam.tileID + tileY * 32 + tileX;

            if (tileIdx >= tiles.getSize()) continue;

            renderTile(drawList, xPos, yPos, zoom, tileIdx, tx, ty,
                paletteIndex, oam.hFlip, oam.vFlip, alpha);
        }
    }
}

void Sofanthiel::renderTile(ImDrawList* drawList, float xPos, float yPos, float zoom,
    int tileIdx, int tx, int ty, int paletteIndex,
    bool hFlip, bool vFlip, float alpha) {
    TileData tile = tiles.getTile(tileIdx);

    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int pixelX = hFlip ? (7 - px) : px;
            int pixelY = vFlip ? (7 - py) : py;

            uint8_t colorIdx = tile.data[pixelY][pixelX];

            if (colorIdx == 0) continue;

            SDL_Color color = palettes[paletteIndex].colors[colorIdx];

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
void Sofanthiel::handleCelOAMs() {
    ImGui::Begin("OAMs", nullptr, ImGuiWindowFlags_NoCollapse);

    if (editingCelIndex < 0 || editingCelIndex >= animationCels.size()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No cel selected for editing");
        ImGui::End();
        return;
    }

    AnimationCel& cel = animationCels[editingCelIndex];

    if (ImGui::Button("Add OAM")) {
        TengokuOAM newOAM;
        newOAM.xPosition = 0;
        newOAM.yPosition = 0;
        newOAM.tileID = 0;
        newOAM.palette = 0;
        newOAM.objShape = SHAPE_SQUARE;
        newOAM.objSize = 0;
        newOAM.hFlip = false;
        newOAM.vFlip = false;

        cel.oams.push_back(newOAM);

        selectedOAMIndices.clear();
        selectedOAMIndices.push_back(cel.oams.size() - 1);
    }

    ImGui::Separator();

    ImGui::BeginChild("OAMsList", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    static int draggedItem = -1;
    static int dragHoverItem = -1;

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

        if (i == dragHoverItem) {
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
                std::sort(selectedOAMIndices.begin(), selectedOAMIndices.end(), std::greater<int>());

                for (int index : selectedOAMIndices) {
                    if (index >= 0 && index < cel.oams.size()) {
                        cel.oams.erase(cel.oams.begin() + index);
                    }
                }

                selectedOAMIndices.clear();
            }

            ImGui::EndPopup();
        }

        if (i == dragHoverItem) {
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            itemInteracted = true;
            ImGui::SetDragDropPayload("DND_OAM_INDEX", &i, sizeof(int));
            ImGui::Text("Moving OAM %d", i);
            draggedItem = i;
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            itemInteracted = true;
            dragHoverItem = i;

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

                draggedItem = -1;
                dragHoverItem = -1;
            }

            ImGui::EndDragDropTarget();
        }
        else if (draggedItem >= 0 && i != draggedItem) {
            if (ImGui::IsMouseHoveringRect(
                ImGui::GetItemRectMin(),
                ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + 3))) {
                dragHoverItem = i;
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

            draggedItem = -1;
            dragHoverItem = -1;
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !itemInteracted) {
        selectedOAMIndices.clear();
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        draggedItem = -1;
        dragHoverItem = -1;
    }

    ImGui::EndChild();
    ImGui::End();
}
void Sofanthiel::handleCelEditor() {
    ImGui::Begin("Cel Editor", nullptr, ImGuiWindowFlags_NoCollapse);

    if (editingCelIndex < 0 || editingCelIndex >= animationCels.size()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No cel selected for editing");
        ImGui::End();
        return;
    }

    AnimationCel& cel = animationCels[editingCelIndex];

    if (selectedOAMIndices.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No OAM selected");
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

    float infoHeight = ImGui::GetTextLineHeightWithSpacing() * 2.5 + ImGui::GetStyle().ItemSpacing.y * 2;
    float contentHeight = ImGui::GetContentRegionAvail().y - infoHeight - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("CelSpritesheetContent");

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
		currentPalette = selectedOAMIndices.size() > 0 ? this->animationCels[editingCelIndex].oams[selectedOAMIndices[0]].palette : 0;
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

void Sofanthiel::drawTimelineControls()
{
    if (ImGui::Button(ICON_FA_BACKWARD_FAST)) { // Jump to start
        currentFrame = 0;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_BACKWARD_STEP)) {
        currentFrame = (currentFrame > 0) ? currentFrame - 1 : totalFrames > 0 ? totalFrames - 1 : 0;
    }
    ImGui::SameLine();

    if (ImGui::Button(isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
        isPlaying = !isPlaying;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FORWARD_STEP) && totalFrames > 0) {
        currentFrame = (currentFrame + 1) % totalFrames;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FORWARD_FAST)) { // Jump to end
        currentFrame = totalFrames > 0 ? totalFrames - 1 : 0;
    }
    ImGui::SameLine();

    ImGui::Text("Frame: %d/%d", currentFrame, totalFrames == 0 ? 0 : totalFrames - 1);

    ImGui::SameLine(calculateRightAlignedPosition("Loop", getScaledSize(130)));
    ImGui::Checkbox("Loop", &loopAnimation);

    ImGui::SameLine(calculateRightAlignedPosition(getScaledSize(150), getScaledSize(10)));
    ImGui::SetNextItemWidth(getScaledSize(150));
    ImGui::SliderFloat("FPS", &frameRate, 1.0f, 120.0f, "%.1f");
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
    static TimelineResizeState resizeState;
    static TimelineDragState dragState;
    static int hoveredEntryIdx = -1;
    static std::vector<int> selectedEntryIndices;
    static std::vector<AnimationEntry> clipboardEntries;
    
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
        hoveredEntryIdx = -1;

        if (ImGui::IsWindowFocused()) {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !selectedEntryIndices.empty()) {
                clipboardEntries.clear();
                for (int idx : selectedEntryIndices) {
                    if (idx >= 0 && idx < anim.entries.size()) {
                        clipboardEntries.push_back(anim.entries[idx]);
                    }
                }
            }

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !clipboardEntries.empty()) {
                int insertPos = selectedEntryIndices.empty() ? anim.entries.size() :
                    *std::max_element(selectedEntryIndices.begin(), selectedEntryIndices.end()) + 1;

                anim.entries.insert(anim.entries.begin() + insertPos,
                    clipboardEntries.begin(), clipboardEntries.end());

                selectedEntryIndices.clear();
                for (size_t i = 0; i < clipboardEntries.size(); i++) {
                    selectedEntryIndices.push_back(insertPos + i);
                }

                totalFrames = 0;
                for (const AnimationEntry& e : anim.entries) {
                    totalFrames += e.duration;
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                selectedEntryIndices.clear();
            }
        }

        if (!dragState.isDragging) {
            drawTimelineEntries(anim, drawList, winPos, syncScroll, frameWidth, resizeState, selectedEntryIndices, timelineEntryHeight, clipboardEntries);
        }
        else {
            int frameIndex = 0;
            for (int entryIdx = 0; entryIdx < anim.entries.size(); entryIdx++) {
                if (entryIdx == dragState.draggedEntryIdx) {
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

                bool isSelected = std::find(selectedEntryIndices.begin(), selectedEntryIndices.end(), entryIdx) != selectedEntryIndices.end();

                drawTimelineEntryBackground(drawList, winPos, syncScroll, entryIdx, celStartX, celWidth, isSelected, selectedEntryIndices, hoveredEntryIdx, clipboardEntries, timelineEntryHeight);
                drawTimelineEntryLabel(drawList, winPos, syncScroll, entry.celName, celStartX, celWidth, timelineEntryHeight);
                drawTimelineFrameCels(drawList, winPos, syncScroll, frameIndex, entry.duration, frameWidth, timelineEntryHeight);

                frameIndex += entry.duration;
            }
        }

        handleTimelineResizing(anim, resizeState, syncScroll, frameWidth);
        if (!resizeState.isResizing) { handleTimelineDragging(anim, drawList, winPos, syncScroll, frameWidth, dragState, timelineEntryHeight); }

        handleTimelineAutoScroll(syncScroll, frameWidth);

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && !selectedEntryIndices.empty()) {
            std::sort(selectedEntryIndices.begin(), selectedEntryIndices.end(), std::greater<int>());

            for (int idx : selectedEntryIndices) {
                if (idx >= 0 && idx < anim.entries.size()) {
                    anim.entries.erase(anim.entries.begin() + idx);
                }
            }

            selectedEntryIndices.clear();

            totalFrames = 0;
            for (const AnimationEntry& e : anim.entries) {
                totalFrames += e.duration;
            }

            if (totalFrames > 0) {
                currentFrame = std::min(currentFrame, totalFrames - 1);
            }
            else {
                currentFrame = 0;
            }
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

                    totalFrames = 0;
                    for (const AnimationEntry& e : anim.entries) {
                        totalFrames += e.duration;
                    }
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
    static int hoveredEntryIdx = -1;

    for (int entryIdx = 0; entryIdx < anim.entries.size(); entryIdx++) {
        AnimationEntry& entry = anim.entries[entryIdx];

        float celStartX = frameIndex * frameWidth;
        float celWidth = entry.duration * frameWidth;

        if (celStartX - syncScroll + celWidth < 0 || celStartX - syncScroll > ImGui::GetWindowWidth()) {
            frameIndex += entry.duration;
            continue;
        }

        bool isSelected = std::find(selectedEntryIndices.begin(), selectedEntryIndices.end(), entryIdx) != selectedEntryIndices.end();

        drawTimelineEntryBackground(drawList, winPos, syncScroll, entryIdx, celStartX, celWidth, isSelected, selectedEntryIndices, hoveredEntryIdx, clipboardEntries, entryHeight);
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
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !selectedEntryIndices.empty())) {
                clipboardEntries.clear();
                for (int idx : selectedEntryIndices) {
                    if (idx >= 0 && idx < anim.entries.size()) {
                        clipboardEntries.push_back(anim.entries[idx]);
                    }
                }
            }

            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !clipboardEntries.empty())) {
                int insertPos = *std::max_element(selectedEntryIndices.begin(), selectedEntryIndices.end()) + 1;

                anim.entries.insert(anim.entries.begin() + insertPos,
                    clipboardEntries.begin(), clipboardEntries.end());

                selectedEntryIndices.clear();
                for (size_t i = 0; i < clipboardEntries.size(); i++) {
                    selectedEntryIndices.push_back(insertPos + i);
                }

                totalFrames = 0;
                for (const AnimationEntry& e : anim.entries) {
                    totalFrames += e.duration;
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", "Del", false, !selectedEntryIndices.empty())) {
                std::sort(selectedEntryIndices.begin(), selectedEntryIndices.end(), std::greater<int>());

                for (int idx : selectedEntryIndices) {
                    if (idx >= 0 && idx < anim.entries.size()) {
                        anim.entries.erase(anim.entries.begin() + idx);
                    }
                }

                selectedEntryIndices.clear();

                totalFrames = 0;
                for (const AnimationEntry& e : anim.entries) {
                    totalFrames += e.duration;
                }

                if (totalFrames > 0) {
                    currentFrame = std::min(currentFrame, totalFrames - 1);
                }
                else {
                    currentFrame = 0;
                }
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

            totalFrames = 0;
            for (const AnimationEntry& e : anim.entries) {
                totalFrames += e.duration;
            }

            currentFrame = std::min(currentFrame, totalFrames - 1);
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
        std::vector<std::pair<int, int>> framePositions; // pair of <frame_pos, insert_idx>
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
                    ImVec2(insertX, winPos.y + 27),
                    IM_COL32(255, 255, 0, 255),
                    2.0f
                );

                const float triangleSize = 6.0f;
                ImVec2 trianglePoints[3];

                // top triangle
                trianglePoints[0] = ImVec2(insertX - triangleSize, winPos.y - 2);
                trianglePoints[1] = ImVec2(insertX + triangleSize, winPos.y - 2);
                trianglePoints[2] = ImVec2(insertX, winPos.y - 2 + triangleSize);
                drawList->AddTriangleFilled(
                    trianglePoints[0], trianglePoints[1], trianglePoints[2],
                    IM_COL32(255, 255, 0, 255)
                );

                // bottom triangle
                trianglePoints[0] = ImVec2(insertX - triangleSize, winPos.y + 27);
                trianglePoints[1] = ImVec2(insertX + triangleSize, winPos.y + 27);
                trianglePoints[2] = ImVec2(insertX, winPos.y + 27 - triangleSize);
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

                totalFrames = 0;
                for (const AnimationEntry& e : anim.entries) {
                    totalFrames += e.duration;
                }

                currentFrame = std::min(currentFrame, totalFrames - 1);
            }

            dragState.isDragging = false;
            dragState.draggedEntryIdx = -1;
            dragState.targetInsertIdx = -1;
        }
    }
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

    if (cel == nullptr) return;

    float offsetX = previewSize.x / 2.0f + previewAnimationOffset.x;
    float offsetY = previewSize.y / 2.0f + previewAnimationOffset.y;

    // Render OAMs in reverse order so that earlier OAMs are drawn on top (proper z-ordering)
    for (int i = cel->oams.size() - 1; i >= 0; i--) {
        const TengokuOAM& oam = cel->oams[i];
        renderOAM(drawList, origin, zoom, oam, offsetX, offsetY, 1.0f);
    }
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
    ImGui::ColorEdit3("Background Color", view.backgroundColor, ImGuiColorEditFlags_NoInputs);

    ImGui::SameLine(calculateRightAlignedPosition("Reset View", getScaledSize(10)));
    if (ImGui::Button("Reset View")) {
        view.resetView();
    }
    
    ImGui::Text("Zoom: %.0f%% - Cursor: (%.0f, %.0f) - Offset: (%.0f, %.0f)",
        view.zoom * 100,
        SDL_clamp(floor(mousePosInWindow.x / view.zoom), 0, contentSize.x),
        SDL_clamp(floor(mousePosInWindow.y / view.zoom), 0, contentSize.y),
        view.offset.x, view.offset.y);
    
    // Calculate the total width needed for the right-aligned controls
    float checkboxWidth = ImGui::CalcTextSize("Use Palette BG Color").x + getScaledSize(16); // checkbox size + text
    float paletteTextWidth = ImGui::CalcTextSize(" Palette").x;
    float sliderWidth = getScaledSize(55);
    float spacing = ImGui::GetStyle().ItemSpacing.x * 2; // spacing between items
    float totalRightWidth = checkboxWidth + paletteTextWidth + sliderWidth + spacing;
    
    ImGui::SameLine(calculateRightAlignedPosition(totalRightWidth, getScaledSize(10)));
    ImGui::Checkbox("Use Palette BG Color", &usePaletteBGColor);
    ImGui::SameLine();
    ImGui::Text(" Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(getScaledSize(55));
    if (palettes.size() > 0) {
        ImGui::SliderInt("##Palette", &currentPalette, 0, this->palettes.size() - 1);
        currentPalette = SDL_clamp(currentPalette, 0, this->palettes.size() - 1);
    } else {
        ImGui::Text("No palettes");
    }
}

void Sofanthiel::handlePalette()
{
    ImGui::Begin("Palette", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button("Add Palette") && palettes.size() < 16) {
        Palette newPalette;
        for (int i = 0; i < 16; i++) {
            Uint8 gray = (Uint8)(i * 255 / 15);
            newPalette.colors[i] = { gray, gray, gray, 255 };
        }
        palettes.push_back(newPalette);
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove Palette") && palettes.size() > 0) {
        palettes.pop_back();
        currentPalette = SDL_clamp(currentPalette, 0, (int)palettes.size() - 1);
    }

    ImGui::SameLine();
    ImGui::Text("Palettes: %d/16", (int)palettes.size());

    ImGui::Separator();

    float infoHeight = ImGui::GetTextLineHeightWithSpacing() * 3.5 + ImGui::GetStyle().ItemSpacing.y * 3;
    float contentHeight = ImGui::GetContentRegionAvail().y - infoHeight - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("PaletteContent", ImVec2(0, contentHeight));

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 baseSize = ImVec2(16 * 16, 16 * palettes.size());
    ImVec2 origin = paletteView.calculateOrigin(contentCenter, baseSize);

    drawPaletteContent(origin);

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("PaletteInfo");

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

    float totalWidth = 16.0f * 16 * paletteView.zoom;  // 16 colors × 16 pixels each
    float totalHeight = 16.0f * palettes.size() * paletteView.zoom;  // 16 pixels height × number of palettes

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

    static int editPaletteIdx = 0;
    static int editColorIdx = 0;
    static float editColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

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

    // Calculate the actual used area
    float usedWidth = celSize * paletteWidth;
    float usedHeight = celSize * palettes.size();

    ImVec2 gridOrigin = ImVec2(
        origin.x + (scaledSize.x - usedWidth) * 0.5f,
        origin.y + (scaledSize.y - usedHeight) * 0.5f
    );

    // Vertical grid lines
    for (int x = 0; x <= paletteWidth; x++) {
        float xPos = gridOrigin.x + x * celSize;
        drawList->AddLine(
            ImVec2(xPos, gridOrigin.y),
            ImVec2(xPos, gridOrigin.y + usedHeight),
            gridColor
        );
    }

    // Horizontal grid lines
    for (int y = 0; y <= palettes.size(); y++) {
        float yPos = gridOrigin.y + y * celSize;
        drawList->AddLine(
            ImVec2(gridOrigin.x, yPos),
            ImVec2(gridOrigin.x + usedWidth, yPos),
            gridColor
        );
    }
}

void Sofanthiel::drawPaletteInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin)
{
    ImGui::SameLine(calculateRightAlignedPosition("Reset View", getScaledSize(10)));
    if (ImGui::Button("Reset View")) {
        view.resetView();
    }

    ImGui::Text("Zoom: %.0f%% - Cursor: (%.0f, %.0f) - Offset: (%.0f, %.0f)",
        view.zoom * 100,
        SDL_clamp(floor((mousePosInWindow.x / view.zoom / 16)), 0, 15),
        SDL_clamp(floor((mousePosInWindow.y / view.zoom / 16)), 0, palettes.size() - 1),
        view.offset.x, view.offset.y);
}

void Sofanthiel::getOAMDimensions(int objShape, int objSize, int& width, int& height) {
    switch (objShape) {
    case SHAPE_SQUARE: // Square
        switch (objSize) {
        case 0: width = 8; height = 8; break;
        case 1: width = 16; height = 16; break;
        case 2: width = 32; height = 32; break;
        case 3: width = 64; height = 64; break;
        }
        break;
    case SHAPE_HORIZONTAL: // Horizontal
        switch (objSize) {
        case 0: width = 16; height = 8; break;
        case 1: width = 32; height = 8; break;
        case 2: width = 32; height = 16; break;
        case 3: width = 64; height = 32; break;
        }
        break;
    case SHAPE_VERTICAL: // Vertical
        switch (objSize) {
        case 0: width = 8; height = 16; break;
        case 1: width = 8; height = 32; break;
        case 2: width = 16; height = 32; break;
        case 3: width = 32; height = 64; break;
        }
        break;
    }
}

void Sofanthiel::handlePreview()
{
    ImGui::Begin("Preview", nullptr, ImGuiWindowFlags_NoCollapse);

    float infoHeight = ImGui::GetTextLineHeightWithSpacing() * 2.5 + ImGui::GetStyle().ItemSpacing.y * 2;
    float contentHeight = ImGui::GetContentRegionAvail().y - infoHeight - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("PreviewContent", ImVec2(0, contentHeight));

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 origin = previewView.calculateOrigin(contentCenter, previewSize);

    drawPreviewContent(origin);

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("PreviewInfo");

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

void Sofanthiel::handleSpritesheet()
{
    ImGui::Begin("Spritesheet", nullptr, ImGuiWindowFlags_NoCollapse);

    float infoHeight = ImGui::GetTextLineHeightWithSpacing() * 2.5 + ImGui::GetStyle().ItemSpacing.y * 2;
    float contentHeight = ImGui::GetContentRegionAvail().y - infoHeight - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("SpritesheetContent", ImVec2(0, contentHeight));

    ImVec2 contentCenter = calculateContentCenter();
    ImVec2 baseSize = ImVec2(tiles.getWidth(), tiles.getHeight());
    ImVec2 origin = spritesheetView.calculateOrigin(contentCenter, baseSize);

    drawSpritesheetContent(origin);

    ImGui::EndChild();
    ImGui::Separator();

    ImGui::BeginChild("SpritesheetInfo");

    ImVec2 mousePosInWindow = ImVec2(
        ImGui::GetIO().MousePos.x - origin.x,
        ImGui::GetIO().MousePos.y - origin.y
    );

    drawSpritesheetInfoPanel(spritesheetView, mousePosInWindow, baseSize, origin);

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::handleAnimCels()
{
    ImGui::Begin("Animation Cels", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Available Animation Cels");

    if (ImGui::Button("New Animation Cel")) {
        showNewCelPopup = true;
        memset(newCelNameBuffer, 0, sizeof(newCelNameBuffer));
    }

    ImGui::Separator();

    // Handle New Cel Popup
    if (showNewCelPopup) {
        ImGui::OpenPopup("New Animation Cel");
    }

    if (ImGui::BeginPopupModal("New Animation Cel", &showNewCelPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter a name for the new animation cel:");
        ImGui::InputText("##celname", newCelNameBuffer, sizeof(newCelNameBuffer));

        bool nameValid = strlen(newCelNameBuffer) > 0;
        bool nameExists = false;

        if (nameValid) {
            for (const auto& cel : animationCels) {
                if (cel.name == newCelNameBuffer) {
                    nameExists = true;
                    break;
                }
            }
        }

        if (nameExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "A cel with this name already exists!");
            ImGui::BeginDisabled(true);
        }
        else if (!nameValid) {
            ImGui::BeginDisabled(true);
        }

        if (ImGui::Button("Create", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            AnimationCel newCel;
            newCel.name = newCelNameBuffer;
            animationCels.push_back(newCel);

            showNewCelPopup = false;
            ImGui::CloseCurrentPopup();
        }

        if (nameExists || !nameValid) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showNewCelPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Handle Rename Cel Popup
    if (showRenameCelPopup) {
        ImGui::OpenPopup("Rename Animation Cel");
    }

    if (ImGui::BeginPopupModal("Rename Animation Cel", &showRenameCelPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (renamingCelIndex >= 0 && renamingCelIndex < animationCels.size()) {
            ImGui::Text("Enter new name for '%s':", animationCels[renamingCelIndex].name.c_str());
            ImGui::InputText("##renamecel", renameCelNameBuffer, sizeof(renameCelNameBuffer));

            bool nameValid = strlen(renameCelNameBuffer) > 0;
            bool nameExists = false;

            if (nameValid) {
                for (size_t i = 0; i < animationCels.size(); i++) {
                    if (i != renamingCelIndex && animationCels[i].name == renameCelNameBuffer) {
                        nameExists = true;
                        break;
                    }
                }
            }

            if (nameExists) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "A cel with this name already exists!");
                ImGui::BeginDisabled(true);
            }
            else if (!nameValid) {
                ImGui::BeginDisabled(true);
            }

            if (ImGui::Button("Rename", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                std::string oldName = animationCels[renamingCelIndex].name;
                std::string newName = renameCelNameBuffer;

                animationCels[renamingCelIndex].name = newName;

                for (auto& anim : animations) {
                    for (auto& entry : anim.entries) {
                        if (entry.celName == oldName) {
                            entry.celName = newName;
                        }
                    }
                }

                showRenameCelPopup = false;
                renamingCelIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            if (nameExists || !nameValid) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showRenameCelPopup = false;
                renamingCelIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    ImGui::BeginChild("AnimationCelsList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    float buttonHeight = getScaledSize(20.0f);
    float availWidth = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < animationCels.size(); i++) {
        const AnimationCel& cel = animationCels[i];

        ImGui::PushID(i);

        bool clicked = ImGui::Button(cel.name.c_str(), ImVec2(availWidth, buttonHeight));

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            this->celEditingMode = true;
            this->editingCelIndex = i;
            this->selectedOAMIndices.clear();
        }

        if (ImGui::BeginPopupContextItem("##cel_context")) {
            if (ImGui::MenuItem("Rename")) {
                showRenameCelPopup = true;
                renamingCelIndex = i;
                strncpy(renameCelNameBuffer, animationCels[i].name.c_str(), sizeof(renameCelNameBuffer) - 1);
                renameCelNameBuffer[sizeof(renameCelNameBuffer) - 1] = '\0';
            }
            if (ImGui::MenuItem("Copy Cel")) {
                celClipboard = cel;
                hasCelClipboard = true;
            }

            if (ImGui::MenuItem("Paste Cel", nullptr, false, hasCelClipboard)) {
                AnimationCel pastedCel = celClipboard;
                std::string baseName = pastedCel.name;
                std::string newName = baseName;
                int counter = 1;

                bool nameExists;
                do {
                    nameExists = false;
                    for (const auto& existingCel : animationCels) {
                        if (existingCel.name == newName) {
                            nameExists = true;
                            newName = baseName + "_" + std::to_string(counter++);
                            break;
                        }
                    }
                } while (nameExists);

                pastedCel.name = newName;
                animationCels.push_back(pastedCel);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Remove Cel")) {
                animationCels.erase(animationCels.begin() + i);
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            int payload_n = i;
            ImGui::SetDragDropPayload("DND_ANIM_CELL", &payload_n, sizeof(int));
            ImGui::Text("%s", cel.name.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
    }

    if (ImGui::IsWindowFocused() && editingCelIndex >= 0 && editingCelIndex < animationCels.size()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
            celClipboard = animationCels[editingCelIndex];
            hasCelClipboard = true;
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && hasCelClipboard) {
            AnimationCel pastedCel = celClipboard;

            std::string baseName = pastedCel.name;
            std::string newName = baseName;
            int counter = 1;

            bool nameExists;
            do {
                nameExists = false;
                for (const auto& existingCel : animationCels) {
                    if (existingCel.name == newName) {
                        nameExists = true;
                        newName = baseName + "_" + std::to_string(counter++);
                        break;
                    }
                }
            } while (nameExists);

            pastedCel.name = newName;
            animationCels.push_back(pastedCel);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

// Implementation of renaming animations
void Sofanthiel::handleAnims()
{
    ImGui::Begin("Animations", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Available Animations");

    if (ImGui::Button("New Animation")) {
        showNewAnimationPopup = true;
        memset(newAnimationNameBuffer, 0, sizeof(newAnimationNameBuffer));
    }

    ImGui::Separator();

    // Handle New Animation Popup
    if (showNewAnimationPopup) {
        ImGui::OpenPopup("New Animation");
    }

    if (ImGui::BeginPopupModal("New Animation", &showNewAnimationPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter a name for the new animation:");
        ImGui::InputText("##animname", newAnimationNameBuffer, sizeof(newAnimationNameBuffer));

        bool nameValid = strlen(newAnimationNameBuffer) > 0;
        bool nameExists = false;

        if (nameValid) {
            for (const auto& anim : animations) {
                if (anim.name == newAnimationNameBuffer) {
                    nameExists = true;
                    break;
                }
            }
        }

        if (nameExists) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "An animation with this name already exists!");
            ImGui::BeginDisabled(true);
        }
        else if (!nameValid) {
            ImGui::BeginDisabled(true);
        }

        if (ImGui::Button("Create", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            Animation newAnimation;
            newAnimation.name = newAnimationNameBuffer;
            animations.push_back(newAnimation);

            showNewAnimationPopup = false;
            ImGui::CloseCurrentPopup();
        }

        if (nameExists || !nameValid) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showNewAnimationPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Handle Rename Animation Popup
    if (showRenameAnimationPopup) {
        ImGui::OpenPopup("Rename Animation");
    }

    if (ImGui::BeginPopupModal("Rename Animation", &showRenameAnimationPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (renamingAnimationIndex >= 0 && renamingAnimationIndex < animations.size()) {
            ImGui::Text("Enter new name for '%s':", animations[renamingAnimationIndex].name.c_str());
            ImGui::InputText("##renameanim", renameAnimationNameBuffer, sizeof(renameAnimationNameBuffer));

            bool nameValid = strlen(renameAnimationNameBuffer) > 0;
            bool nameExists = false;

            if (nameValid) {
                for (size_t i = 0; i < animations.size(); i++) {
                    if (i != renamingAnimationIndex && animations[i].name == renameAnimationNameBuffer) {
                        nameExists = true;
                        break;
                    }
                }
            }

            if (nameExists) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "An animation with this name already exists!");
                ImGui::BeginDisabled(true);
            }
            else if (!nameValid) {
                ImGui::BeginDisabled(true);
            }

            if (ImGui::Button("Rename", getScaledButtonSize(120, 0)) || (nameValid && !nameExists && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                animations[renamingAnimationIndex].name = renameAnimationNameBuffer;

                showRenameAnimationPopup = false;
                renamingAnimationIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            if (nameExists || !nameValid) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", getScaledButtonSize(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showRenameAnimationPopup = false;
                renamingAnimationIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    ImGui::BeginChild("AnimationsList", ImVec2(0, getScaledSize(40)), false,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);

    float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
    float xPosition = 0.0f;

    ImGui::BeginGroup();
    for (int i = 0; i < animations.size(); i++) {
        Animation& anim = animations[i];

        ImVec2 textSize = ImGui::CalcTextSize(anim.name.c_str());
        float buttonWidth = textSize.x + getScaledSize(40.0f);

        if (i > 0) {
            ImGui::SameLine();
        }

        bool isSelected = (currentAnimation == i);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
        }

        if (ImGui::Button(anim.name.c_str(), ImVec2(buttonWidth, getScaledSize(20.0f)))) {
            currentAnimation = i;

            totalFrames = 0;
            for (const AnimationEntry& entry : animations[currentAnimation].entries) {
                totalFrames += entry.duration;
            }

            if (totalFrames > 0) {
                currentFrame = std::min(currentFrame, totalFrames - 1);
            }
            else {
                currentFrame = 0;
            }
        }

        if (ImGui::BeginPopupContextItem(("##anim_context_" + std::to_string(i)).c_str())) {
            if (ImGui::MenuItem("Rename")) {
                showRenameAnimationPopup = true;
                renamingAnimationIndex = i;
                strncpy(renameAnimationNameBuffer, animations[i].name.c_str(), sizeof(renameAnimationNameBuffer) - 1);
                renameAnimationNameBuffer[sizeof(renameAnimationNameBuffer) - 1] = '\0';
            }
            if (ImGui::MenuItem("Copy Animation")) {
                animationClipboard = anim;
                hasAnimationClipboard = true;
            }

            if (ImGui::MenuItem("Paste Animation", nullptr, false, hasAnimationClipboard)) {
                Animation pastedAnimation = animationClipboard;

                std::string baseName = pastedAnimation.name;
                std::string newName = baseName;
                int counter = 1;

                bool nameExists;
                do {
                    nameExists = false;
                    for (const auto& existingAnim : animations) {
                        if (existingAnim.name == newName) {
                            nameExists = true;
                            newName = baseName + "_" + std::to_string(counter++);
                            break;
                        }
                    }
                } while (nameExists);

                pastedAnimation.name = newName;
                animations.push_back(pastedAnimation);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Remove Animation")) {
                animations.erase(animations.begin() + i);
                if (currentAnimation >= animations.size()) {
                    currentAnimation = animations.empty() ? -1 : animations.size() - 1;
                }
            }

            ImGui::EndPopup();
        }

        if (isSelected) {
            ImGui::PopStyleColor(3);
        }

        xPosition += buttonWidth + buttonSpacing;
    }
    ImGui::EndGroup();

    if (ImGui::IsWindowFocused() && currentAnimation >= 0 && currentAnimation < animations.size()) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
            animationClipboard = animations[currentAnimation];
            hasAnimationClipboard = true;
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && hasAnimationClipboard) {
            Animation pastedAnimation = animationClipboard;

            std::string baseName = pastedAnimation.name;
            std::string newName = baseName;
            int counter = 1;

            bool nameExists;
            do {
                nameExists = false;
                for (const auto& existingAnim : animations) {
                    if (existingAnim.name == newName) {
                        nameExists = true;
                        newName = baseName + "_" + std::to_string(counter++);
                        break;
                    }
                }
            } while (nameExists);

            pastedAnimation.name = newName;
            animations.push_back(pastedAnimation);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

void Sofanthiel::drawGrid(ImDrawList* drawList, ImVec2 origin, ImVec2 size, float zoom) {
    if (!showGrid) return;

    float gridSpacing = gridSize * zoom;
    ImU32 gridColor = IM_COL32(70, 70, 70, 200);

    // vertical lines
    for (float x = origin.x + gridSpacing; x < origin.x + size.x; x += gridSpacing) {
        drawList->AddLine(
            ImVec2(x, origin.y),
            ImVec2(x, origin.y + size.y),
            gridColor
        );
    }

    // horizontal lines
    for (float y = origin.y + gridSpacing; y < origin.y + size.y; y += gridSpacing) {
        drawList->AddLine(
            ImVec2(origin.x, y),
            ImVec2(origin.x + size.x, y),
            gridColor
        );
    }
}

void Sofanthiel::drawBackground(ImDrawList* drawList, ImVec2 origin, ImVec2 size, float* color) {
    drawList->AddRectFilled(
        origin,
        ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(color[0] * 255, color[1] * 255, color[2] * 255, color[3] * 255)
    );
}

ImVec2 Sofanthiel::calculateContentCenter() {
    ImVec2 winSize = ImGui::GetContentRegionAvail();
    ImVec2 winPos = ImGui::GetCursorScreenPos();

    return ImVec2(
        winPos.x + winSize.x * 0.5f,
        winPos.y + winSize.y * 0.5f
    );
}

float Sofanthiel::calculateRightAlignedPosition(const char* text, float padding) {
    float textWidth = ImGui::CalcTextSize(text).x;
    return ImGui::GetWindowWidth() - textWidth - getScaledSize(padding);
}

float Sofanthiel::calculateRightAlignedPosition(float elementWidth, float padding) {
    return ImGui::GetWindowWidth() - elementWidth - getScaledSize(padding);
}

float Sofanthiel::getScaledSize(float baseSize) {
    return baseSize * dpiScale;
}

ImVec2 Sofanthiel::getScaledButtonSize(float baseWidth, float baseHeight) {
    return ImVec2(getScaledSize(baseWidth), getScaledSize(baseHeight));
}

void Sofanthiel::drawPreviewInfoPanel(ViewManager& view, ImVec2 mousePosInWindow, ImVec2 contentSize, const ImVec2& origin)
{
    ImGui::ColorEdit4("Background Color", view.backgroundColor, ImGuiColorEditFlags_NoInputs);

    // Calculate positions for right-aligned elements
    float resetButtonWidth = ImGui::CalcTextSize("Reset View").x + getScaledSize(16); // button padding
    float overscanWidth = ImGui::CalcTextSize("Show Overscan").x + getScaledSize(16);
    float backgroundWidth = ImGui::CalcTextSize("Show Background").x + getScaledSize(16);
    float separatorWidth = getScaledSize(16);
    
    float totalRightWidth = resetButtonWidth + overscanWidth + backgroundWidth + (separatorWidth * 2);
    
    ImGui::SameLine(calculateRightAlignedPosition(totalRightWidth, getScaledSize(10)));
    if (ImGui::Checkbox("Show Background", &showBackgroundTexture)) {
        this->previewView.backgroundColor[3] = !showBackgroundTexture ? 1.0f : 0.5f;
    }
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Checkbox("Show Overscan", &showOverscanArea);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        view.resetView();
        previewAnimationOffset = ImVec2(0, 0);
    }

    ImGui::Text("Zoom: %.0f%% - Cursor: (%.0f, %.0f) - Offset: (%.0f, %.0f)",
        view.zoom * 100,
        SDL_clamp(floor(mousePosInWindow.x / view.zoom), 0, contentSize.x),
        SDL_clamp(floor(mousePosInWindow.y / view.zoom), 0, contentSize.y),
        view.offset.x, view.offset.y);

    // Animation offset controls on the right
    float animOffsetWidth = getScaledSize(70) + ImGui::CalcTextSize("Animation Offset:").x;
	ImGui::SameLine(calculateRightAlignedPosition(animOffsetWidth, getScaledSize(10)));
    ImGui::Text("Animation Offset:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(getScaledSize(70));
	ImGui::DragFloat2("##AnimOffset", (float*)&previewAnimationOffset, 1.0f, -128.0f, 127.0f, "%.0f");
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

    previewAnimationOffset.x = SDL_clamp(previewAnimationOffset.x, -128.0f, 127.0f);
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

    for (const TengokuOAM& oam : cel->oams) {
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

            int tileIdx = oam.tileID + tileY * 32 + tileX;
            if (tileIdx >= tiles.getSize()) continue;

            int pixelX = localX % 8;
            int pixelY = localY % 8;

            if (oam.hFlip) pixelX = 7 - pixelX;
            if (oam.vFlip) pixelY = 7 - pixelY;

            TileData tile = tiles.getTile(tileIdx);
            uint8_t colorIdx = tile.data[pixelY][pixelX];

            return true;
        }
    }

    return false;
}

void ViewManager::handleZoomAndPan(bool isHovered, ImVec2 origin) {
    // zooming
    if (isHovered && ImGui::GetIO().MouseWheel != 0) {
        float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
        zoom = ImMax(0.1f, zoom + zoomDelta);
    }

    // panning
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && isHovered) {
        if (!isPanning) {
            isPanning = true;
            panStartPos = ImGui::GetIO().MousePos;
            startOffset = offset;
        }

        ImVec2 delta = ImVec2(
            ImGui::GetIO().MousePos.x - panStartPos.x,
            ImGui::GetIO().MousePos.y - panStartPos.y
        );

        offset = ImVec2(
            startOffset.x + delta.x / zoom,
            startOffset.y + delta.y / zoom
        );
    }
    else {
        isPanning = false;
    }
}

void ViewManager::resetView() {
    zoom = 1.0f;
    offset = ImVec2(0.0f, 0.0f);
}

ImVec2 ViewManager::getScaledSize(ImVec2 baseSize) {
    return ImVec2(baseSize.x * zoom, baseSize.y * zoom);
}

ImVec2 ViewManager::calculateOrigin(ImVec2 contentCenter, ImVec2 baseSize) {
    ImVec2 scaledSize = getScaledSize(baseSize);
    return ImVec2(
        contentCenter.x - scaledSize.x * 0.5f + offset.x * zoom,
        contentCenter.y - scaledSize.y * 0.5f + offset.y * zoom
    );
}

void Sofanthiel::handleDPIChange() {
    float newDisplayScale = SDL_GetWindowDisplayScale(this->window);
    if (newDisplayScale <= 0.0f) {
        newDisplayScale = 1.0f;
    }
    
    // Only update if scale actually changed
    if (newDisplayScale != this->dpiScale) {
        SDL_Log("DPI scale changed from %.2f to %.2f", this->dpiScale, newDisplayScale);
        
        float scaleRatio = newDisplayScale / this->dpiScale;
        this->dpiScale = newDisplayScale;
        
        // Update ImGui style scaling
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scaleRatio);
        
        // Rebuild fonts with new scale
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        
        float baseFontSize = 13.0f * newDisplayScale;
        float iconFontSize = baseFontSize * 2.0f / 3.0f;
        
        // Load default font with proper size
        ImFontConfig defaultConfig;
        defaultConfig.SizePixels = baseFontSize;
        defaultConfig.OversampleH = 2;
        defaultConfig.OversampleV = 1;
        defaultConfig.PixelSnapH = true;
        io.Fonts->AddFontDefault(&defaultConfig);
        
        // Merge in icons from Font Awesome with proper scaling
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.OversampleH = 2;
        icons_config.OversampleV = 1;
        icons_config.GlyphMinAdvanceX = iconFontSize;
        
        // Try to load FontAwesome icons, but don't fail if the file is missing
        ImFont* iconFont = io.Fonts->AddFontFromFileTTF("assets/" FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges);
        if (iconFont == nullptr) {
            SDL_Log("Warning: Could not load FontAwesome icons from assets/" FONT_ICON_FILE_NAME_FAS);
        }
        
        io.Fonts->Build();
        
        // Force ImGui to recreate device objects
        ImGui_ImplSDLRenderer3_DestroyDeviceObjects();
        ImGui_ImplSDLRenderer3_CreateDeviceObjects();
    }
}