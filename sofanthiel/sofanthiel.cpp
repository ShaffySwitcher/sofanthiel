#include "Sofanthiel.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"
#include "UndoRedo.h"

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

    this->window = SDL_CreateWindow("Sofanthiel (v2.00)", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (this->window == nullptr)
    {
        SDL_Log("Error creating window: %s\n", SDL_GetError());
        return false;
    }

    SDL_Surface* iconSurface = IMG_Load("assets/icon.png");
    if (iconSurface) {
        SDL_SetWindowIcon(this->window, iconSurface);
        SDL_DestroySurface(iconSurface);
    }

    int logicalW = 0, pixelW = 0;
    SDL_GetWindowSize(this->window, &logicalW, nullptr);
    SDL_GetWindowSizeInPixels(this->window, &pixelW, nullptr);
    float displayScale = (logicalW > 0) ? ((float)pixelW / (float)logicalW) : 1.0f;
    if (displayScale < 1.0f) displayScale = 1.0f;
    SDL_Log("Display scale factor: %.2f (logical %d, pixel %d)", displayScale, logicalW, pixelW);

	this->renderer = SDL_CreateRenderer(this->window, NULL);
    if (this->renderer == nullptr) {
        SDL_Log("Error creating renderer: %s\n", SDL_GetError());
		return false;
    }
    SDL_SetRenderScale(this->renderer, displayScale, displayScale);
	SDL_SetRenderVSync(this->renderer, true);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    int windowWidth = 0, windowHeight = 0;
    SDL_GetWindowSize(this->window, &windowWidth, &windowHeight);
    io.DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);
    io.DisplayFramebufferScale = ImVec2(displayScale, displayScale);

    this->dpiScale = displayScale;

    ImGuiStyle& style = ImGui::GetStyle();
    this->applyTheme();
    style.ScaleAllSizes(displayScale);

	ImGui_ImplSDL3_InitForSDLRenderer(this->window, this->renderer);
    if(!ImGui_ImplSDLRenderer3_Init(this->renderer)) {
        SDL_Log("Error initializing ImGui renderer: %s\n", SDL_GetError());
        return false;
	}

    float baseFontSize = 13.0f * displayScale;
    float iconFontSize = baseFontSize * 2.0f / 3.0f;

    io.Fonts->Clear();

    ImFontConfig defaultConfig;
    defaultConfig.SizePixels = baseFontSize;
    defaultConfig.OversampleH = 2;
    defaultConfig.OversampleV = 1;
    defaultConfig.PixelSnapH = true;
    io.Fonts->AddFontDefault(&defaultConfig);

    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 2;
    icons_config.OversampleV = 1;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    
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

    if (this->backgroundTexture != nullptr) {
        SDL_DestroyTexture(this->backgroundTexture);
        this->backgroundTexture = nullptr;
    }

    if (this->ssImportPreviewTex != nullptr) {
        SDL_DestroyTexture(this->ssImportPreviewTex);
        this->ssImportPreviewTex = nullptr;
    }

    if(ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
	}

    if (this->renderer != nullptr) {
        SDL_DestroyRenderer(this->renderer);
        this->renderer = nullptr;
	}

    if (this->window != nullptr) {
        SDL_DestroyWindow(this->window);
        this->window = nullptr;
    }

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
        else if(event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
                event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            handleDPIChange();
        }
        else if(event.type == SDL_EVENT_DROP_FILE) {
            const char* droppedFile = event.drop.data;
            if (droppedFile) {
                handleDroppedFile(std::string(droppedFile));
            }
        }
	}

    if (InputManager::isPressed(InputManager::PlayPause) && !ImGui::GetIO().WantCaptureKeyboard && !this->celEditingMode) {
        isPlaying = !isPlaying;
    }
}

void Sofanthiel::handleDroppedFile(const std::string& path)
{
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        SDL_Log("Dropped file has no extension: %s", path.c_str());
        return;
    }

    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    SDL_Log("Dropped file: %s (ext: %s)", path.c_str(), ext.c_str());

    if (ext == "inv") {
        loadProject(path);
    }
    else if (ext == "4bpp" || ext == "bin") {
        this->tiles = ResourceManager::loadTiles(path);
    }
    else if (ext == "png" || ext == "bmp" || ext == "jpg" || ext == "jpeg") {
        this->tiles = ResourceManager::loadTilesFromImageAndPalette(path, this->palettes, this->currentPalette);
    }
    else if (ext == "pal") {
        this->palettes = ResourceManager::loadPalettes(path);
        this->currentPalette = SDL_clamp(this->currentPalette, 0,
            static_cast<int>(this->palettes.size()) - 1);
    }
    else if (ext == "c") {
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();

        if (content.find("TO_RGB555") != std::string::npos && content.find("Palette") != std::string::npos) {
            parsedPaletteGroups = ResourceManager::parsePalettesFromCFile(path);
            if (!parsedPaletteGroups.empty()) {
                paletteImportSelections.clear();
                for (const auto& group : parsedPaletteGroups) {
                    paletteImportSelections.push_back(
                        std::vector<uint8_t>(group.palettes.size(), 1));
                }
                showPaletteImportPopup = true;
            }
        }
        else if (content.find("AnimationCel") != std::string::npos) {
            this->animationCels = ResourceManager::loadAnimationCels(path);
            std::string filename = path.substr(path.find_last_of("/\\") + 1);
            this->animationCelFilename = filename;
        }
        else if (content.find("struct Animation") != std::string::npos || content.find("END_ANIMATION") != std::string::npos) {
            this->animations = ResourceManager::loadAnimations(path);
        }
        else {
            SDL_Log("Unrecognized .c file format: %s", path.c_str());
        }
    }
    else {
        SDL_Log("Unsupported file dropped: %s", path.c_str());
    }
}

void Sofanthiel::update()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    int logicalW = 0, logicalH = 0, pixelW = 0, pixelH = 0;
    SDL_GetWindowSize(this->window, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(this->window, &pixelW, &pixelH);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)logicalW, (float)logicalH);
    io.DisplayFramebufferScale = ImVec2(
        (logicalW > 0) ? ((float)pixelW / (float)logicalW) : 1.0f,
        (logicalH > 0) ? ((float)pixelH / (float)logicalH) : 1.0f
    );

    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(ImGui::GetID("Sofanthiel"), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    
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

    if (showPaletteImportPopup) {
        ImGui::OpenPopup("Import Palettes");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(350, 200), ImVec2(600, 500));
    }

    if (ImGui::BeginPopupModal("Import Palettes", &showPaletteImportPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        int totalParsed = 0;
        for (const auto& g : parsedPaletteGroups)
            totalParsed += static_cast<int>(g.palettes.size());

        ImGui::TextWrapped(
            "Found %zu group(s) containing %d palette(s) in the C file.",
            parsedPaletteGroups.size(), totalParsed);

        if (!this->palettes.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.4f, 1.0f),
                "You currently have %zu palette(s) loaded.",
                this->palettes.size());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        int totalSelected = 0;
        int totalSelectable = 0;
        for (const auto& sel : paletteImportSelections) {
            for (auto s : sel) { if (s) totalSelected++; totalSelectable++; }
        }

        if (ImGui::SmallButton("Select All")) {
            for (auto& sel : paletteImportSelections)
                for (auto& s : sel) s = 1;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect All")) {
            for (auto& sel : paletteImportSelections)
                for (auto& s : sel) s = 0;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d / %d selected", totalSelected, totalSelectable);

        ImGui::BeginChild("PaletteList", ImVec2(0, 300), ImGuiChildFlags_Borders);

        for (size_t gi = 0; gi < parsedPaletteGroups.size(); ++gi) {
            const auto& group = parsedPaletteGroups[gi];
            auto& selections = paletteImportSelections[gi];

            int selectedCount = 0;
            for (auto s : selections) if (s) selectedCount++;

            ImGui::PushID(static_cast<int>(gi));

            if (ImGui::TreeNodeEx("##group", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap,
                "%s (%d / %zu selected)", group.name.c_str(), selectedCount, group.palettes.size())) {

                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("All  None").x - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::SmallButton("All")) {
                    for (size_t i = 0; i < selections.size(); ++i) selections[i] = 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("None")) {
                    for (size_t i = 0; i < selections.size(); ++i) selections[i] = 0;
                }

                for (size_t pi = 0; pi < group.palettes.size(); ++pi) {
                    ImGui::PushID(static_cast<int>(pi));

                    char label[64];
                    snprintf(label, sizeof(label), "Palette %02d", static_cast<int>(pi));
                    bool checked = selections[pi] != 0;
                    if (ImGui::Checkbox(label, &checked)) {
                        selections[pi] = checked ? 1 : 0;
                    }

                    ImGui::SameLine();
                    ImVec2 swatchStart = ImGui::GetCursorScreenPos();
                    float swatchSize = ImGui::GetTextLineHeight();
                    ImDrawList* dl = ImGui::GetWindowDrawList();

                    for (int ci = 0; ci < 16; ++ci) {
                        const SDL_Color& c = group.palettes[pi].colors[ci];
                        ImVec2 p0(swatchStart.x + ci * (swatchSize + 1), swatchStart.y);
                        ImVec2 p1(p0.x + swatchSize, p0.y + swatchSize);
                        dl->AddRectFilled(p0, p1, IM_COL32(c.r, c.g, c.b, 255));
                        dl->AddRect(p0, p1, IM_COL32(60, 60, 60, 255));
                    }

                    ImGui::Dummy(ImVec2(16 * (swatchSize + 1), swatchSize));

                    ImGui::PopID();
                }

                ImGui::TreePop();
            }
            else {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("All  None").x - ImGui::GetStyle().ItemSpacing.x);
                if (ImGui::SmallButton("All")) {
                    for (size_t i = 0; i < selections.size(); ++i) selections[i] = 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("None")) {
                    for (size_t i = 0; i < selections.size(); ++i) selections[i] = 0;
                }
            }

            ImGui::PopID();
        }

        ImGui::EndChild();

        totalSelected = 0;
        for (const auto& sel : paletteImportSelections)
            for (auto s : sel) if (s) totalSelected++;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canImport = totalSelected > 0;
        if (!canImport) ImGui::BeginDisabled();

        if (ImGui::Button(ICON_FA_FILE_IMPORT " Replace All", getScaledButtonSize(130, 0))) {
            this->palettes.clear();
            for (size_t gi = 0; gi < parsedPaletteGroups.size(); ++gi) {
                for (size_t pi = 0; pi < parsedPaletteGroups[gi].palettes.size(); ++pi) {
                    if (paletteImportSelections[gi][pi]) {
                        this->palettes.push_back(parsedPaletteGroups[gi].palettes[pi]);
                    }
                }
            }
            this->currentPalette = 0;
            showPaletteImportPopup = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Clear all current palettes and import only the selected ones");
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PLUS " Append", getScaledButtonSize(100, 0))) {
            for (size_t gi = 0; gi < parsedPaletteGroups.size(); ++gi) {
                for (size_t pi = 0; pi < parsedPaletteGroups[gi].palettes.size(); ++pi) {
                    if (paletteImportSelections[gi][pi]) {
                        this->palettes.push_back(parsedPaletteGroups[gi].palettes[pi]);
                    }
                }
            }
            showPaletteImportPopup = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Add selected palettes after your existing ones");
        }

        if (!canImport) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK " Cancel", getScaledButtonSize(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showPaletteImportPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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
    ImGuiID dockspaceID = ImGui::GetID("Sofanthiel");
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID);
    ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetIO().DisplaySize);

    ImGuiID dockIDMain = dockspaceID;
    ImGuiID dockIDBottom = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Down, 0.25f, nullptr, &dockIDMain);
    ImGuiID dockIDTop = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Up, 0.15f, nullptr, &dockIDMain);
    ImGuiID dockIDLeft = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Left, 0.18f, nullptr, &dockIDMain);
    ImGuiID dockIDRight = ImGui::DockBuilderSplitNode(dockIDMain, ImGuiDir_Right, 0.22f, nullptr, &dockIDMain);
    
	// Main windows
    ImGui::DockBuilderDockWindow("Preview", dockIDMain);
	ImGui::DockBuilderDockWindow("Spritesheet", dockIDMain);
	ImGui::DockBuilderDockWindow("Palette", dockIDMain);
    ImGui::DockBuilderDockWindow("Timeline", dockIDBottom);
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
            if (ImGui::MenuItem(ICON_FA_FILE " New", "Ctrl+N")) {
                this->animationCels.clear();
                this->animations.clear();
                this->palettes.clear();
                this->tiles = Tiles();
                this->celEditingMode = false;
                this->editingCelIndex = -1;
				this->selectedOAMIndices.clear();
                this->undoManager.clear();

                this->initializeDefaultPalettes();
                this->currentPalette = 0;
            }
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open", "Ctrl+O", nullptr, true)) {
                nfdresult_t result = NFD_OpenDialog("inv", nullptr, &outPath);
                if (result == NFD_OKAY) {
                    loadProject(std::string(outPath));
                    free(outPath);
                }
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S", nullptr, true)) {
                if (!currentProjectPath.empty()) {
                    saveProject(currentProjectPath);
                } else {
                    nfdresult_t result = NFD_SaveDialog("inv", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        std::string savePath(outPath);
                        free(outPath);
                        if (savePath.find('.') == std::string::npos ||
                            savePath.substr(savePath.find_last_of('.')) != ".inv") {
                            savePath += ".inv";
                        }
                        saveProject(savePath);
                    }
                }
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...", "Ctrl+Shift+S", nullptr, true)) {
                nfdresult_t result = NFD_SaveDialog("inv", nullptr, &outPath);
                if (result == NFD_OKAY) {
                    std::string savePath(outPath);
                    free(outPath);
                    if (savePath.find('.') == std::string::npos ||
                        savePath.substr(savePath.find_last_of('.')) != ".inv") {
                        savePath += ".inv";
                    }
                    saveProject(savePath);
                }
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
                if (ImGui::MenuItem(ICON_FA_PALETTE " Palette (.pal, .c)")) {
                    nfdresult_t result = NFD_OpenDialog("pal,c", nullptr, &outPath);

                    if (result == NFD_OKAY) {
                        std::string outPathStr(outPath);
                        if(outPathStr.substr(outPathStr.find_last_of(".") + 1) == "pal") {
                            this->palettes = ResourceManager::loadPalettes(outPath);
                        } else {
                            parsedPaletteGroups = ResourceManager::parsePalettesFromCFile(outPath);
                            if (!parsedPaletteGroups.empty()) {
                                paletteImportSelections.clear();
                                for (const auto& group : parsedPaletteGroups) {
                                    paletteImportSelections.push_back(
                                        std::vector<uint8_t>(group.palettes.size(), 1));
                                }
                                showPaletteImportPopup = true;
                            }
                        }
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
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_WAND_MAGIC_SPARKLES " Convert Image to Spritesheet + Palette")) {
                    nfdresult_t result = NFD_OpenDialog("png,bmp,jpg,jpeg", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        Tiles newTiles;
                        std::vector<Palette> newPalettes;
                        if (ResourceManager::convertImageToSpritesheetAndPalette(outPath, newTiles, newPalettes)) {
                            this->tiles = newTiles;
                            this->palettes = newPalettes;
                            this->currentPalette = 0;
                        }
                        free(outPath);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Load an image and auto-extract a 16-color palette + spritesheet");
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
                ImGui::Separator();
                bool canExportGif = currentAnimation >= 0 &&
                    currentAnimation < static_cast<int>(animations.size()) &&
                    !animations[currentAnimation].entries.empty() &&
                    !animationCels.empty() && tiles.getSize() > 0;
                if (ImGui::BeginMenu(ICON_FA_SLIDERS " GIF Settings")) {
                    if (ImGui::MenuItem("Scale x1", nullptr, gifExportScale == 1)) gifExportScale = 1;
                    if (ImGui::MenuItem("Scale x2", nullptr, gifExportScale == 2)) gifExportScale = 2;
                    if (ImGui::MenuItem("Scale x3", nullptr, gifExportScale == 3)) gifExportScale = 3;
                    if (ImGui::MenuItem("Scale x4", nullptr, gifExportScale == 4)) gifExportScale = 4;
                    ImGui::EndMenu();
                }
                if (!canExportGif) ImGui::BeginDisabled();
                if (ImGui::MenuItem(ICON_FA_FILM " Animation to GIF (.gif)")) {
                    nfdresult_t result = NFD_SaveDialog("gif", nullptr, &outPath);
                    if (result == NFD_OKAY) {
                        std::string savePath(outPath);
                        free(outPath);
                        if (savePath.find('.') == std::string::npos ||
                            savePath.substr(savePath.find_last_of('.')) != ".gif") {
                            savePath += ".gif";
                        }
                        float offX = previewSize.x / 2.0f + previewAnimationOffset.x;
                        float offY = previewSize.y / 2.0f + previewAnimationOffset.y;
                        ResourceManager::exportAnimationToGif(savePath,
                            this->animations, this->currentAnimation,
                            this->animationCels, this->tiles, this->palettes,
                            this->frameRate,
                            static_cast<int>(previewSize.x),
                            static_cast<int>(previewSize.y),
                            offX, offY, gifExportScale);
                    }
                }
                if (!canExportGif) ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    if (!canExportGif)
                        ImGui::SetTooltip("Select an animation with cels and tiles loaded first");
                    else
                        ImGui::SetTooltip("Export current animation as an animated GIF (scale x%d)", gifExportScale);
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
            if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT " Undo", "Ctrl+Z", false, undoManager.canUndo())) {
                undoManager.undo();
            }
            if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " Redo", "Ctrl+Y", false, undoManager.canRedo())) {
                undoManager.redo();
            }
            ImGui::Separator();

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

    // Global keyboard shortcuts
    if (InputManager::isPressed(InputManager::Undo)) {
        undoManager.undo();
    }
    if (InputManager::isPressed(InputManager::Redo) || InputManager::isPressed(InputManager::RedoAlt)) {
        undoManager.redo();
    }
    if (InputManager::isPressed(InputManager::Save)) {
        if (!currentProjectPath.empty()) {
            saveProject(currentProjectPath);
        }
    }
    if (InputManager::isPressed(InputManager::Open)) {
        nfdresult_t result = NFD_OpenDialog("inv", nullptr, &outPath);
        if (result == NFD_OKAY) {
            loadProject(std::string(outPath));
            free(outPath);
        }
    }

    if (this->celEditingMode && editingCelIndex >= 0 && editingCelIndex < animationCels.size()) {
        bool hasSelection = !selectedOAMIndices.empty();

        if (InputManager::isPressed(InputManager::Copy) && hasSelection) {
            AnimationCel& cel = animationCels[editingCelIndex];
            oamClipboard.clear();
            for (int index : selectedOAMIndices) {
                if (index >= 0 && index < cel.oams.size()) {
                    oamClipboard.push_back(cel.oams[index]);
                }
            }
        }

        if (InputManager::isPressed(InputManager::Paste) &&
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

void Sofanthiel::recalculateTotalFrames() {
    totalFrames = 0;
    if (currentAnimation >= 0 && currentAnimation < static_cast<int>(animations.size())) {
        for (const AnimationEntry& entry : animations[currentAnimation].entries) {
            totalFrames += entry.duration;
        }
    }
    if (totalFrames > 0) {
        currentFrame = std::min(currentFrame, totalFrames - 1);
    } else {
        currentFrame = 0;
    }
}

bool Sofanthiel::isCelNameUnique(const std::string& name, int excludeIndex) const {
    for (size_t i = 0; i < animationCels.size(); i++) {
        if (static_cast<int>(i) != excludeIndex && animationCels[i].name == name) {
            return false;
        }
    }
    return true;
}

bool Sofanthiel::isAnimationNameUnique(const std::string& name, int excludeIndex) const {
    for (size_t i = 0; i < animations.size(); i++) {
        if (static_cast<int>(i) != excludeIndex && animations[i].name == name) {
            return false;
        }
    }
    return true;
}

void Sofanthiel::applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // background
    colors[ImGuiCol_WindowBg]             = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.11f, 0.11f, 0.13f, 0.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.13f, 0.13f, 0.16f, 0.98f);

    // borders
    colors[ImGuiCol_Border]               = ImVec4(0.24f, 0.24f, 0.28f, 0.50f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // frame bg
    colors[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.28f, 0.36f, 1.00f);

    // title bar
    colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.09f, 0.09f, 0.11f, 0.75f);

    // menu bar
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);

    // scrollbar
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.09f, 0.09f, 0.11f, 0.40f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.28f, 0.33f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.38f, 0.45f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.48f, 0.48f, 0.55f, 1.00f);

    // checkmarks and sliders
    colors[ImGuiCol_CheckMark]            = ImVec4(0.48f, 0.68f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.38f, 0.54f, 0.78f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.50f, 0.68f, 0.92f, 1.00f);

    // button
    colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.34f, 0.46f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.34f, 0.44f, 0.60f, 1.00f);

    // header
    colors[ImGuiCol_Header]               = ImVec4(0.20f, 0.24f, 0.32f, 0.70f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.34f, 0.46f, 0.80f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.34f, 0.44f, 0.60f, 1.00f);

    // separators
    colors[ImGuiCol_Separator]            = ImVec4(0.22f, 0.22f, 0.26f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.38f, 0.54f, 0.78f, 0.78f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.38f, 0.54f, 0.78f, 1.00f);

    // resize
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.28f, 0.34f, 0.46f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.38f, 0.54f, 0.78f, 0.60f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.38f, 0.54f, 0.78f, 0.90f);

    // tabs
    colors[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.28f, 0.34f, 0.48f, 0.90f);
    colors[ImGuiCol_TabSelected]          = ImVec4(0.20f, 0.26f, 0.38f, 1.00f);
    colors[ImGuiCol_TabDimmed]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.14f, 0.18f, 0.26f, 1.00f);

    // docker
    colors[ImGuiCol_DockingPreview]       = ImVec4(0.38f, 0.54f, 0.78f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);

    // text
    colors[ImGuiCol_Text]                 = ImVec4(0.88f, 0.88f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);

    // table
    colors[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]    = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_TableBorderLight]     = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);

    // i haven o fucking idea how to categorize these
    style.WindowRounding    = 5.0f;
    style.ChildRounding     = 3.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(8.0f, 6.0f);
    style.FramePadding      = ImVec2(6.0f, 3.0f);
    style.CellPadding       = ImVec2(4.0f, 2.0f);
    style.ItemSpacing       = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 11.0f;
    style.GrabMinSize       = 8.0f;

    style.TabBarBorderSize  = 1.0f;
    style.TabBarOverlineSize = 2.0f;
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

static void writeU32(std::ofstream& f, uint32_t val) {
    f.write(reinterpret_cast<const char*>(&val), 4);
}

static uint32_t readU32(std::ifstream& f) {
    uint32_t val = 0;
    f.read(reinterpret_cast<char*>(&val), 4);
    return val;
}

void Sofanthiel::saveProject(const std::string& path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        SDL_Log("Failed to save project to: %s", path.c_str());
        return;
    }

    uint32_t sectionCount = 0;
    if (tiles.getSize() > 0) sectionCount++;
    if (!palettes.empty()) sectionCount++;
    if (!animationCels.empty()) sectionCount++;
    if (!animations.empty()) sectionCount++;
    sectionCount++;

    // Header
    file.write("ENOT", 4); // ENOT RAIN WORLDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD (this is the only thing keeping me sane at this point)
    writeU32(file, 1);     // version
    writeU32(file, sectionCount);

    // raw 4bpp - section 1
    if (tiles.getSize() > 0) {
        writeU32(file, 1);

        std::vector<uint8_t> tileBytes;
        for (int i = 0; i < tiles.getSize(); ++i) {
            TileData td = tiles.getTile(i);
            for (int byteIndex = 0; byteIndex < 32; ++byteIndex) {
                int py = byteIndex / 4;
                int px = (byteIndex % 4) * 2;
                uint8_t pixel1 = td.data[py][px] & 0x0F;
                uint8_t pixel2 = td.data[py][px + 1] & 0x0F;
                tileBytes.push_back(pixel1 | (pixel2 << 4));
            }
        }

        writeU32(file, static_cast<uint32_t>(tileBytes.size()));
        file.write(reinterpret_cast<const char*>(tileBytes.data()), tileBytes.size());
    }

    // raw palette data - section 2
    if (!palettes.empty()) {
        writeU32(file, 2);

        // RGBA!!!!!!!!!!!!!!!!!
        uint32_t dataSize = static_cast<uint32_t>(palettes.size() * 16 * 4);
        writeU32(file, dataSize);

        for (const auto& pal : palettes) {
            for (int i = 0; i < 16; ++i) {
                file.write(reinterpret_cast<const char*>(&pal.colors[i].r), 1);
                file.write(reinterpret_cast<const char*>(&pal.colors[i].g), 1);
                file.write(reinterpret_cast<const char*>(&pal.colors[i].b), 1);
                file.write(reinterpret_cast<const char*>(&pal.colors[i].a), 1);
            }
        }
    }

    // anim cels as text - section 3
    if (!animationCels.empty()) {
        writeU32(file, 3); // type

        std::ostringstream oss;
        for (const auto& cel : animationCels) {
            oss << "AnimationCel " << cel.name << "[] = {\n";
            oss << "    /* Len */ " << cel.oams.size() << ",\n";
            for (size_t i = 0; i < cel.oams.size(); ++i) {
                const auto& oam = cel.oams[i];
                const uint16_t* raw = reinterpret_cast<const uint16_t*>(&oam);
                oss << "    /* " << std::setw(3) << std::setfill('0') << i << " */ ";
                oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << raw[0] << ", ";
                oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << raw[1] << ", ";
                oss << "0x" << std::hex << std::setw(4) << std::setfill('0') << raw[2];
                oss << std::dec;
                if (i + 1 < cel.oams.size()) oss << ",";
                oss << "\n";
            }
            oss << "};\n\n";
        }
        std::string text = oss.str();
        writeU32(file, static_cast<uint32_t>(text.size()));
        file.write(text.c_str(), text.size());
    }

    // animations as text - section 4
    if (!animations.empty()) {
        writeU32(file, 4); // type

        std::ostringstream oss;
        oss << "#include \"global.h\"\n#include \"graphics.h\"\n\n";
        oss << "#include \"" << animationCelFilename << "\"\n\n";
        for (const auto& anim : animations) {
            oss << "struct Animation " << anim.name << "[] = {\n";
            for (size_t i = 0; i < anim.entries.size(); ++i) {
                const auto& entry = anim.entries[i];
                oss << "    /* " << std::setw(3) << std::setfill('0') << i << " */ { "
                    << entry.celName << ", " << static_cast<int>(entry.duration) << " },\n";
            }
            oss << "    /* " << std::setw(3) << std::setfill('0') << anim.entries.size() << " */ END_ANIMATION,\n";
            oss << "};\n\n";
        }
        std::string text = oss.str();
        writeU32(file, static_cast<uint32_t>(text.size()));
        file.write(text.c_str(), text.size());
    }

    // metadata - section 5
    writeU32(file, 5); // type

    std::ostringstream oss;
    oss << "celFilename=" << animationCelFilename << "\n";
    oss << "currentPalette=" << currentPalette << "\n";
    oss << "currentAnimation=" << currentAnimation << "\n";
    oss << "frameRate=" << frameRate << "\n";
    oss << "loopAnimation=" << (loopAnimation ? 1 : 0) << "\n";

    std::string text = oss.str();
    writeU32(file, static_cast<uint32_t>(text.size()));
    file.write(text.c_str(), text.size());

    file.close();
    this->currentProjectPath = path;
    SDL_Log("Saved project to %s", path.c_str());
}

void Sofanthiel::loadProject(const std::string& path)
{
    std::string celsText;
    std::string animsText;
    std::string metadataText;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        SDL_Log("Failed to open project: %s", path.c_str());
        return;
    }

    char magic[4];
    file.read(magic, 4);
    if (memcmp(magic, "ENOT", 4) != 0) {
        SDL_Log("Invalid project file (bad magic): %s", path.c_str());
        file.close();
        return;
    }

    uint32_t version = readU32(file);
    if (version != 1) {
        SDL_Log("Unsupported project version %u in: %s", version, path.c_str());
        file.close();
        return;
    }

    uint32_t sectionCount = readU32(file);

    // reset everything
    this->tiles = Tiles();
    this->palettes.clear();
    this->animationCels.clear();
    this->animations.clear();
    this->celEditingMode = false;
    this->editingCelIndex = -1;
    this->selectedOAMIndices.clear();
    this->currentAnimation = -1;
    this->currentAnimationCel = -1;
    this->currentFrame = 0;
    this->isPlaying = false;
    this->undoManager.clear();

    for (uint32_t s = 0; s < sectionCount; ++s) {
        if (!file.good()) break;

        uint32_t sectionType = readU32(file);
        uint32_t dataLen = readU32(file);

        if (sectionType == 1) { // 4bpp
            std::vector<uint8_t> tileData(dataLen);
            file.read(reinterpret_cast<char*>(tileData.data()), dataLen);

            this->tiles = Tiles();
            for (size_t offset = 0; offset + 32 <= tileData.size(); offset += 32) {
                std::array<uint8_t, 32> tile;
                memcpy(tile.data(), tileData.data() + offset, 32);
                this->tiles.addTile(tile);
            }
        }
        else if (sectionType == 2) { // palette
            std::vector<uint8_t> palData(dataLen);
            file.read(reinterpret_cast<char*>(palData.data()), dataLen);

            this->palettes.clear();
            size_t offset = 0;
            while (offset + (16*4) <= palData.size()) {
                Palette pal;
                for (int i = 0; i < 16; ++i) {
                    pal.colors[i].r = palData[offset++];
                    pal.colors[i].g = palData[offset++];
                    pal.colors[i].b = palData[offset++];
                    pal.colors[i].a = palData[offset++];
                }
                this->palettes.push_back(pal);
            }
        }
        else if (sectionType == 3) { // animation cels
            celsText.resize(dataLen);
            file.read(&celsText[0], dataLen);
        }
        else if (sectionType == 4) { // animations
            animsText.resize(dataLen);
            file.read(&animsText[0], dataLen);
        }
        else if (sectionType == 5) { // metadata
            metadataText.resize(dataLen);
            file.read(&metadataText[0], dataLen);
        }
        else {
            SDL_Log("unknown section????? type %u in project file: %s", sectionType, path.c_str());
            file.seekg(dataLen, std::ios::cur);
        }
    }

    file.close();

    if (!celsText.empty()) {
        this->animationCels = ResourceManager::loadAnimationCelsFromText(celsText, path + " [section:cels]");
    }

    if (!animsText.empty()) {
        this->animations = ResourceManager::loadAnimationsFromText(animsText, path + " [section:anims]");
    }

    if (!metadataText.empty()) {
        std::istringstream iss(metadataText);
        std::string line;
        while (std::getline(iss, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if (key == "celFilename") animationCelFilename = val;
            else if (key == "currentPalette") {
                try { currentPalette = std::stoi(val); } catch (...) {}
            }
            else if (key == "currentAnimation") {
                try { currentAnimation = std::stoi(val); } catch (...) {}
            }
            else if (key == "frameRate") {
                try { frameRate = std::stof(val); } catch (...) {}
            }
            else if (key == "loopAnimation") {
                loopAnimation = (val == "1");
            }
        }
    }

    // clamp indices to valid ranges
    if (!palettes.empty()) {
        currentPalette = SDL_clamp(currentPalette, 0, static_cast<int>(palettes.size()) - 1);
    }
    if (!animations.empty()) {
        currentAnimation = SDL_clamp(currentAnimation, 0, static_cast<int>(animations.size()) - 1);
    }

    this->currentProjectPath = path;
    this->recalculateTotalFrames();
    SDL_Log("Loaded project from %s (%d tiles, %zu palettes, %zu cels, %zu anims)",
        path.c_str(), tiles.getSize(), palettes.size(),
        animationCels.size(), animations.size());
}

void Sofanthiel::handleDPIChange() {
    int logicalW = 0, pixelW = 0;
    int logicalH = 0, pixelH = 0;
    SDL_GetWindowSize(this->window, &logicalW, nullptr);
    SDL_GetWindowSize(this->window, nullptr, &logicalH);
    SDL_GetWindowSizeInPixels(this->window, &pixelW, nullptr);
    SDL_GetWindowSizeInPixels(this->window, nullptr, &pixelH);

    float scaleX = (logicalW > 0) ? ((float)pixelW / (float)logicalW) : 1.0f;
    float scaleY = (logicalH > 0) ? ((float)pixelH / (float)logicalH) : 1.0f;
    float newDisplayScale = (scaleX + scaleY) * 0.5f;
    if (newDisplayScale < 1.0f) newDisplayScale = 1.0f;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)logicalW, (float)logicalH);
    io.DisplayFramebufferScale = ImVec2(scaleX, scaleY);
    SDL_SetRenderScale(this->renderer, scaleX, scaleY);

    if (newDisplayScale != this->dpiScale) {
        SDL_Log("DPI scale changed from %.2f to %.2f", this->dpiScale, newDisplayScale);
        
        this->dpiScale = newDisplayScale;

        this->applyTheme();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(newDisplayScale);

        io.Fonts->Clear();
        
        float baseFontSize = 13.0f * newDisplayScale;
        float iconFontSize = baseFontSize * 2.0f / 3.0f;

        ImFontConfig defaultConfig;
        defaultConfig.SizePixels = baseFontSize;
        defaultConfig.OversampleH = 2;
        defaultConfig.OversampleV = 1;
        defaultConfig.PixelSnapH = true;
        io.Fonts->AddFontDefault(&defaultConfig);
        
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.OversampleH = 2;
        icons_config.OversampleV = 1;
        icons_config.GlyphMinAdvanceX = iconFontSize;

        ImFont* iconFont = io.Fonts->AddFontFromFileTTF("assets/" FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges);
        if (iconFont == nullptr) {
            SDL_Log("Warning: Could not load FontAwesome icons from assets/" FONT_ICON_FILE_NAME_FAS);
        }
        
        io.Fonts->Build();

        ImGui_ImplSDLRenderer3_DestroyDeviceObjects();
        ImGui_ImplSDLRenderer3_CreateDeviceObjects();
    }
}
