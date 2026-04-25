#include "Sofanthiel.h"
#include "BuildInfo.h"
#include "IconsFontAwesome6.h"
#include "InputManager.h"
#include "UndoRedo.h"
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

namespace {

void* loadFileToHeapBuffer(const char* path, size_t& outSize)
{
    outSize = 0;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return nullptr;
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        return nullptr;
    }

    file.seekg(0, std::ios::beg);

    void* buffer = std::malloc(static_cast<size_t>(fileSize));
    if (buffer == nullptr) {
        return nullptr;
    }

    if (!file.read(static_cast<char*>(buffer), fileSize)) {
        std::free(buffer);
        return nullptr;
    }

    outSize = static_cast<size_t>(fileSize);
    return buffer;
}

struct RomAnimationImportParseResult {
    bool success = false;
    uint32_t animationPointer = 0;
    Animation animation;
    std::vector<AnimationCel> cels;
    std::vector<uint32_t> entryPointers;
    std::vector<uint32_t> celPointers;
    std::string errorMessage;
    std::string warningMessage;
};

std::string trimString(const std::string& value)
{
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

void copyStringToBuffer(char* buffer, size_t bufferSize, const std::string& value)
{
    if (buffer == nullptr || bufferSize == 0) {
        return;
    }

    std::snprintf(buffer, bufferSize, "%s", value.c_str());
}

std::string getFileStem(const std::string& path)
{
    size_t lastSlash = path.find_last_of("/\\");
    size_t nameStart = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    size_t lastDot = path.find_last_of('.');
    if (lastDot == std::string::npos || lastDot < nameStart) {
        lastDot = path.size();
    }

    return path.substr(nameStart, lastDot - nameStart);
}

std::string sanitizeRomImportName(const std::string& value)
{
    std::string sanitized;
    sanitized.reserve(value.size());

    bool lastWasUnderscore = false;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '_') {
            sanitized.push_back(static_cast<char>(ch));
            lastWasUnderscore = false;
        }
        else if (!lastWasUnderscore) {
            sanitized.push_back('_');
            lastWasUnderscore = true;
        }
    }

    sanitized = trimString(sanitized);
    while (!sanitized.empty() && sanitized.front() == '_') {
        sanitized.erase(sanitized.begin());
    }
    while (!sanitized.empty() && sanitized.back() == '_') {
        sanitized.pop_back();
    }

    if (sanitized.empty()) {
        sanitized = "rom";
    }

    return sanitized;
}

std::string formatGbaPointer(uint32_t pointer)
{
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << pointer;
    return oss.str();
}

std::string buildRomImportSuggestedAnimationName(const std::string& romPath, uint32_t animationPointer)
{
    return sanitizeRomImportName(getFileStem(romPath)) + "_" + formatGbaPointer(animationPointer);
}

std::string buildRomImportSuggestedCelPrefix(const std::string& romPath, uint32_t animationPointer)
{
    return buildRomImportSuggestedAnimationName(romPath, animationPointer) + "_cel";
}

std::string buildRomImportCelName(const std::string& celPrefix, int index)
{
    std::ostringstream oss;
    oss << celPrefix << std::setw(3) << std::setfill('0') << index;
    return oss.str();
}

int calculateAnimationTotalFrames(const Animation& anim)
{
    int totalFrames = 0;
    for (const auto& entry : anim.entries) {
        totalFrames += entry.duration;
    }
    return totalFrames;
}

bool shouldUpdateSuggestedBuffer(const char* currentValue, const std::string& previousSuggestedValue)
{
    std::string current = trimString(currentValue == nullptr ? "" : currentValue);
    return current.empty() || current == previousSuggestedValue;
}

bool loadBinaryFile(const std::string& path, std::vector<uint8_t>& outData)
{
    outData.clear();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        return false;
    }

    file.seekg(0, std::ios::beg);
    outData.resize(static_cast<size_t>(fileSize));
    return file.read(reinterpret_cast<char*>(outData.data()), fileSize).good();
}

bool tryReadLittleU16(const std::vector<uint8_t>& data, size_t offset, uint16_t& outValue)
{
    if (offset + sizeof(uint16_t) > data.size()) {
        return false;
    }

    outValue = static_cast<uint16_t>(data[offset]) |
        (static_cast<uint16_t>(data[offset + 1]) << 8);
    return true;
}

bool tryReadLittleU32(const std::vector<uint8_t>& data, size_t offset, uint32_t& outValue)
{
    if (offset + sizeof(uint32_t) > data.size()) {
        return false;
    }

    outValue = static_cast<uint32_t>(data[offset]) |
        (static_cast<uint32_t>(data[offset + 1]) << 8) |
        (static_cast<uint32_t>(data[offset + 2]) << 16) |
        (static_cast<uint32_t>(data[offset + 3]) << 24);
    return true;
}

bool tryConvertRomPointerToOffset(uint32_t pointer, size_t romSize, size_t& outOffset, std::string* outError = nullptr)
{
    constexpr uint32_t kGbaRomBase = 0x08000000;
    outOffset = 0;

    if ((pointer & 0xFF000000) != kGbaRomBase) {
        if (outError != nullptr) {
            *outError = "Pointers must be in the 0x08XXXXXX ROM address range.";
        }
        return false;
    }

    uint32_t rawOffset = pointer - kGbaRomBase;
    if (rawOffset >= romSize) {
        if (outError != nullptr) {
            std::ostringstream oss;
            oss << "Pointer 0x" << formatGbaPointer(pointer) << " points outside the selected ROM.";
            *outError = oss.str();
        }
        return false;
    }

    outOffset = static_cast<size_t>(rawOffset);
    return true;
}

bool tryParseRomAnimationPointerInput(const std::string& input, size_t romSize,
    uint32_t& outPointer, std::string& outError)
{
    constexpr uint32_t kGbaRomBase = 0x08000000;

    outPointer = 0;
    std::string cleaned;
    cleaned.reserve(input.size());

    for (char ch : input) {
        if (!std::isspace(static_cast<unsigned char>(ch)) && ch != '_') {
            cleaned.push_back(ch);
        }
    }

    if (cleaned.empty()) {
        outError = "Enter a ROM offset such as 0x123456 or 08123456.";
        return false;
    }

    if (cleaned.rfind("0x", 0) == 0 || cleaned.rfind("0X", 0) == 0) {
        cleaned.erase(0, 2);
    }

    if (cleaned.empty()) {
        outError = "Enter a ROM offset such as 0x123456 or 08123456.";
        return false;
    }

    if (!std::all_of(cleaned.begin(), cleaned.end(), [](unsigned char ch) { return std::isxdigit(ch) != 0; })) {
        outError = "The ROM offset may only contain hexadecimal digits.";
        return false;
    }

    uint32_t parsedValue = 0;
    try {
        parsedValue = static_cast<uint32_t>(std::stoul(cleaned, nullptr, 16));
    }
    catch (...) {
        outError = "Failed to parse the ROM offset.";
        return false;
    }

    if (cleaned.size() <= 6) {
        if (parsedValue >= romSize) {
            outError = "That ROM offset points outside the selected ROM.";
            return false;
        }

        outPointer = kGbaRomBase + parsedValue;
        return true;
    }

    if (cleaned.size() != 8) {
        outError = "Use either a 6-digit ROM offset (0xXXXXXX) or an 8-digit GBA pointer (08XXXXXX).";
        return false;
    }

    size_t resolvedOffset = 0;
    if (!tryConvertRomPointerToOffset(parsedValue, romSize, resolvedOffset, &outError)) {
        return false;
    }

    outPointer = parsedValue;
    return true;
}

bool tryParseRomCel(const std::vector<uint8_t>& romData, uint32_t celPointer,
    const std::string& celName, AnimationCel& outCel, std::string& outError)
{
    constexpr uint16_t kMaxRomCelOams = 512;

    size_t celOffset = 0;
    if (!tryConvertRomPointerToOffset(celPointer, romData.size(), celOffset, &outError)) {
        return false;
    }

    if ((celOffset % 2) != 0) {
        outError = "Cel data must be aligned to 2 bytes.";
        return false;
    }

    uint16_t oamCount = 0;
    if (!tryReadLittleU16(romData, celOffset, oamCount)) {
        outError = "Could not read the cel OAM count.";
        return false;
    }

    if (oamCount > kMaxRomCelOams) {
        std::ostringstream oss;
        oss << "Cel at 0x" << formatGbaPointer(celPointer)
            << " declares " << oamCount << " OAMs, which is above the safety limit.";
        outError = oss.str();
        return false;
    }

    size_t requiredBytes = sizeof(uint16_t) + static_cast<size_t>(oamCount) * sizeof(TengokuOAM);
    if (celOffset + requiredBytes > romData.size()) {
        outError = "Cel data extends past the end of the ROM.";
        return false;
    }

    outCel = AnimationCel();
    outCel.name = celName;
    outCel.oams.reserve(oamCount);

    size_t readOffset = celOffset + sizeof(uint16_t);
    for (uint16_t i = 0; i < oamCount; ++i) {
        uint16_t rawOam[3] = {};
        if (!tryReadLittleU16(romData, readOffset, rawOam[0]) ||
            !tryReadLittleU16(romData, readOffset + 2, rawOam[1]) ||
            !tryReadLittleU16(romData, readOffset + 4, rawOam[2])) {
            outError = "Could not read one of the OAM entries for the cel.";
            return false;
        }

        TengokuOAM oam = {};
        std::memcpy(&oam, rawOam, sizeof(TengokuOAM));
        outCel.oams.push_back(oam);
        readOffset += sizeof(TengokuOAM);
    }

    return true;
}

RomAnimationImportParseResult parseRomAnimationData(const std::vector<uint8_t>& romData, uint32_t animationPointer,
    const std::string& animationName, const std::string& celPrefix)
{
    constexpr int kMaxRomAnimationEntries = 2048;

    RomAnimationImportParseResult result;
    result.animationPointer = animationPointer;

    size_t animationOffset = 0;
    if (!tryConvertRomPointerToOffset(animationPointer, romData.size(), animationOffset, &result.errorMessage)) {
        return result;
    }

    if ((animationOffset % 4) != 0) {
        result.warningMessage = "Animation data is not 4-byte aligned. Parsing anyway.";
    }

    result.animation.name = animationName;

    std::unordered_map<uint32_t, int> celIndexByPointer;
    int clampedDurationCount = 0;

    for (int entryIndex = 0; entryIndex < kMaxRomAnimationEntries; ++entryIndex) {
        size_t recordOffset = animationOffset + static_cast<size_t>(entryIndex) * 8;

        uint32_t celPointer = 0;
        uint32_t durationValue = 0;
        if (!tryReadLittleU32(romData, recordOffset, celPointer) ||
            !tryReadLittleU32(romData, recordOffset + 4, durationValue)) {
            result.errorMessage = "Animation data ran off the end of the ROM before an end marker was found.";
            result.animation.entries.clear();
            result.cels.clear();
            result.entryPointers.clear();
            result.celPointers.clear();
            return result;
        }

        if (celPointer == 0 && durationValue == 0) {
            if (result.animation.entries.empty()) {
                result.errorMessage = "The animation pointer immediately points to an end marker.";
                return result;
            }

            if (clampedDurationCount > 0) {
                std::ostringstream oss;
                if (!result.warningMessage.empty()) {
                    oss << result.warningMessage << "\n";
                }
                oss << clampedDurationCount << " duration value(s) were clamped to 255 frames.";
                result.warningMessage = oss.str();
            }

            result.success = true;
            return result;
        }

        if (celPointer == 0) {
            std::ostringstream oss;
            oss << "Entry " << entryIndex << " has a null cel pointer.";
            result.errorMessage = oss.str();
            return result;
        }

        if (durationValue == 0) {
            std::ostringstream oss;
            oss << "Entry " << entryIndex << " has a duration of 0.";
            result.errorMessage = oss.str();
            return result;
        }

        int celIndex = -1;
        auto existingCel = celIndexByPointer.find(celPointer);
        if (existingCel == celIndexByPointer.end()) {
            AnimationCel cel;
            std::string celError;
            std::string celName = buildRomImportCelName(celPrefix, static_cast<int>(result.cels.size()));
            if (!tryParseRomCel(romData, celPointer, celName, cel, celError)) {
                std::ostringstream oss;
                oss << "Failed to parse cel at 0x" << formatGbaPointer(celPointer) << ": " << celError;
                result.errorMessage = oss.str();
                return result;
            }

            celIndex = static_cast<int>(result.cels.size());
            celIndexByPointer[celPointer] = celIndex;
            result.cels.push_back(cel);
            result.celPointers.push_back(celPointer);
        }
        else {
            celIndex = existingCel->second;
        }

        AnimationEntry entry;
        entry.celName = result.cels[static_cast<size_t>(celIndex)].name;
        entry.duration = static_cast<uint8_t>(std::min<uint32_t>(durationValue, 255));
        if (durationValue > 255) {
            ++clampedDurationCount;
        }

        result.animation.entries.push_back(entry);
        result.entryPointers.push_back(celPointer);
    }

    result.errorMessage = "The animation did not hit an 8-byte zero terminator before the safety limit.";
    result.animation.entries.clear();
    result.cels.clear();
    result.entryPointers.clear();
    result.celPointers.clear();
    return result;
}

}

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
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

    this->window = SDL_CreateWindow(BuildInfo::kAppName, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

	this->renderer = SDL_CreateRenderer(this->window, NULL);
    if (this->renderer == nullptr) {
        SDL_Log("Error creating renderer: %s\n", SDL_GetError());
		return false;
    }

    SDL_SetRenderScale(this->renderer, 1.0f, 1.0f);
	SDL_SetRenderVSync(this->renderer, true);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    if (char* prefPath = SDL_GetPrefPath("ShaffySwitcher", "Sofanthiel")) {
        this->imguiSettingsPath = std::string(prefPath) + "imgui.ini";
        SDL_free(prefPath);
    }

    if (!this->imguiSettingsPath.empty()) {
        ImGui::LoadIniSettingsFromDisk(this->imguiSettingsPath.c_str());
    }

	ImGui_ImplSDL3_InitForSDLRenderer(this->window, this->renderer);
    if(!ImGui_ImplSDLRenderer3_Init(this->renderer)) {
        SDL_Log("Error initializing ImGui renderer: %s\n", SDL_GetError());
        return false;
	}

    this->applyDisplayScale(this->getCurrentDisplayScale());

    this->updateWindowTitle();

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
        if (!this->imguiSettingsPath.empty()) {
            ImGui::SaveIniSettingsToDisk(this->imguiSettingsPath.c_str());
        }
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
            beginPaletteImport(ResourceManager::parsePalettesFromCFile(path));
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

ImVec2 getWindowFramebufferScale(SDL_Window* window) {
    if (window == nullptr) {
        return ImVec2(1.0f, 1.0f);
    }

    int logicalW = 0;
    int logicalH = 0;
    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSize(window, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);

    float scaleX = (logicalW > 0) ? ((float)pixelW / (float)logicalW) : 1.0f;
    float scaleY = (logicalH > 0) ? ((float)pixelH / (float)logicalH) : 1.0f;

    if (!std::isfinite(scaleX) || scaleX <= 0.0f) {
        scaleX = 1.0f;
    }
    if (!std::isfinite(scaleY) || scaleY <= 0.0f) {
        scaleY = 1.0f;
    }

    return ImVec2(scaleX, scaleY);
}

void syncImGuiDisplayMetrics(SDL_Window* window)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    int logicalW = 0;
    int logicalH = 0;
    SDL_GetWindowSize(window, &logicalW, &logicalH);

    ImVec2 framebufferScale = getWindowFramebufferScale(window);

    if (logicalW > 0 && logicalH > 0) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)logicalW, (float)logicalH);
        io.DisplayFramebufferScale = framebufferScale;
        return;
    }

    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)pixelW, (float)pixelH);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void Sofanthiel::beginPaletteImport(const std::vector<ParsedCPaletteGroup>& groups)
{
    parsedPaletteGroups = groups;
    paletteImportSelections.clear();
    paletteImportSelections.reserve(parsedPaletteGroups.size());

    for (const auto& group : parsedPaletteGroups) {
        paletteImportSelections.push_back(std::vector<uint8_t>(group.palettes.size(), 1));
    }

    showPaletteImportPopup = !parsedPaletteGroups.empty();
    paletteImportPopupPendingOpen = showPaletteImportPopup;
    paletteImportPreviewGroupIndex = 0;
    paletteImportPreviewPaletteIndex = 0;

    for (int gi = 0; gi < static_cast<int>(parsedPaletteGroups.size()); ++gi) {
        if (!parsedPaletteGroups[gi].palettes.empty()) {
            paletteImportPreviewGroupIndex = gi;
            break;
        }
    }
}

void Sofanthiel::handlePaletteImportPopup()
{
    auto clearPaletteImportState = [this]() {
        showPaletteImportPopup = false;
        paletteImportPopupPendingOpen = false;
        parsedPaletteGroups.clear();
        paletteImportSelections.clear();
        paletteImportPreviewGroupIndex = 0;
        paletteImportPreviewPaletteIndex = 0;
    };

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    ImVec2 minWindowSize(getScaledSize(560.0f), getScaledSize(380.0f));
    ImVec2 maxWindowSize(viewport->WorkSize.x * 0.96f, viewport->WorkSize.y * 0.90f);
    ImVec2 desiredSize(
        ImClamp(viewport->WorkSize.x * 0.72f, minWindowSize.x, getScaledSize(920.0f)),
        ImClamp(viewport->WorkSize.y * 0.78f, minWindowSize.y, getScaledSize(720.0f)));

    if (paletteImportPopupPendingOpen) {
        ImGui::OpenPopup("Import Palettes");
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(desiredSize, ImGuiCond_Appearing);
        paletteImportPopupPendingOpen = false;
    }

    ImGui::SetNextWindowSizeConstraints(minWindowSize, maxWindowSize);

    if (parsedPaletteGroups.empty()) {
        clearPaletteImportState();
        return;
    }

    bool keepOpen = showPaletteImportPopup;
    if (ImGui::BeginPopupModal("Import Palettes", &keepOpen, ImGuiWindowFlags_NoSavedSettings)) {
        showPaletteImportPopup = keepOpen;

        auto collectSelectedPalettes = [this]() {
            std::vector<Palette> selected;
            for (size_t gi = 0; gi < parsedPaletteGroups.size(); ++gi) {
                for (size_t pi = 0; pi < parsedPaletteGroups[gi].palettes.size(); ++pi) {
                    if (paletteImportSelections[gi][pi] != 0) {
                        selected.push_back(parsedPaletteGroups[gi].palettes[pi]);
                    }
                }
            }
            return selected;
        };

        auto ensurePreviewSelectionIsValid = [this]() {
            if (parsedPaletteGroups.empty()) {
                paletteImportPreviewGroupIndex = 0;
                paletteImportPreviewPaletteIndex = 0;
                return;
            }

            paletteImportPreviewGroupIndex = ImClamp(
                paletteImportPreviewGroupIndex,
                0,
                static_cast<int>(parsedPaletteGroups.size()) - 1);

            for (int attempt = 0; attempt < static_cast<int>(parsedPaletteGroups.size()); ++attempt) {
                int groupIdx = (paletteImportPreviewGroupIndex + attempt) % static_cast<int>(parsedPaletteGroups.size());
                const auto& group = parsedPaletteGroups[groupIdx];
                if (!group.palettes.empty()) {
                    paletteImportPreviewGroupIndex = groupIdx;
                    paletteImportPreviewPaletteIndex = ImClamp(
                        paletteImportPreviewPaletteIndex,
                        0,
                        static_cast<int>(group.palettes.size()) - 1);
                    return;
                }
            }

            paletteImportPreviewPaletteIndex = 0;
        };

        ensurePreviewSelectionIsValid();

        int totalParsed = 0;
        int totalSelectable = 0;
        int totalSelected = 0;
        for (size_t gi = 0; gi < parsedPaletteGroups.size(); ++gi) {
            totalParsed += static_cast<int>(parsedPaletteGroups[gi].palettes.size());
            totalSelectable += static_cast<int>(paletteImportSelections[gi].size());
            for (uint8_t selected : paletteImportSelections[gi]) {
                totalSelected += (selected != 0) ? 1 : 0;
            }
        }

        std::vector<Palette> selectedPalettes = collectSelectedPalettes();
        int selectedCount = static_cast<int>(selectedPalettes.size());
        int currentPaletteCount = static_cast<int>(palettes.size());
        int appendCapacity = ImMax(0, 16 - currentPaletteCount);
        bool canReplace = selectedCount > 0 && selectedCount <= 16;
        bool canAppend = selectedCount > 0 && selectedCount <= appendCapacity;

        ImGui::TextWrapped(
            "Found %zu group(s) containing %d palette(s). Select the palettes you want to import, then preview them before committing.",
            parsedPaletteGroups.size(), totalParsed);

        if (!palettes.empty()) {
            ImGui::TextColored(
                ImVec4(0.72f, 0.75f, 0.42f, 1.0f),
                "Current project palettes: %d / 16",
                currentPaletteCount);
        }
        else {
            ImGui::TextColored(
                ImVec4(0.55f, 0.70f, 0.90f, 1.0f),
                "Current project palettes: empty");
        }

        if (selectedCount > 16) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                "Too many palettes selected. Sofanthiel supports importing up to 16 at a time.");
        }
        else if (selectedCount > appendCapacity) {
            ImGui::TextColored(
                ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                "Append would exceed the 16-palette limit. Reduce the selection or use Replace All.");
        }
        else {
            ImGui::TextDisabled("%d / %d selected", totalSelected, totalSelectable);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f + getScaledSize(20.0f);
        float listHeight = ImMax(getScaledSize(240.0f), ImGui::GetContentRegionAvail().y - footerHeight);
        float leftPaneWidth = ImMin(getScaledSize(430.0f), ImGui::GetContentRegionAvail().x * 0.62f);

        ImGui::BeginChild("PaletteImportSelectionPane", ImVec2(leftPaneWidth, listHeight), ImGuiChildFlags_Borders);

        if (ImGui::SmallButton("Select All")) {
            for (auto& selections : paletteImportSelections) {
                for (uint8_t& selected : selections) {
                    selected = 1;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect All")) {
            for (auto& selections : paletteImportSelections) {
                for (uint8_t& selected : selections) {
                    selected = 0;
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Hover a palette to preview it");
        ImGui::Separator();

        for (size_t gi = 0; gi < parsedPaletteGroups.size(); ++gi) {
            const auto& group = parsedPaletteGroups[gi];
            auto& selections = paletteImportSelections[gi];

            int selectedInGroup = 0;
            for (uint8_t selected : selections) {
                selectedInGroup += (selected != 0) ? 1 : 0;
            }

            ImGui::PushID(static_cast<int>(gi));
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
            bool opened = ImGui::TreeNodeEx(
                "##PaletteImportGroup",
                nodeFlags,
                "%s  (%d / %zu selected)",
                group.name.c_str(),
                selectedInGroup,
                group.palettes.size());

            ImGui::SameLine();
            if (ImGui::SmallButton("All")) {
                for (uint8_t& selected : selections) {
                    selected = 1;
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("None")) {
                for (uint8_t& selected : selections) {
                    selected = 0;
                }
            }

            if (opened) {
                for (size_t pi = 0; pi < group.palettes.size(); ++pi) {
                    ImGui::PushID(static_cast<int>(pi));

                    bool checked = selections[pi] != 0;
                    bool isPreviewed = paletteImportPreviewGroupIndex == static_cast<int>(gi) &&
                        paletteImportPreviewPaletteIndex == static_cast<int>(pi);

                    if (isPreviewed) {
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.24f, 0.33f, 0.45f, 0.55f));
                    }

                    if (ImGui::Checkbox("##ImportPaletteToggle", &checked)) {
                        selections[pi] = checked ? 1 : 0;
                    }
                    if (ImGui::IsItemHovered()) {
                        paletteImportPreviewGroupIndex = static_cast<int>(gi);
                        paletteImportPreviewPaletteIndex = static_cast<int>(pi);
                    }

                    if (isPreviewed) {
                        ImGui::PopStyleColor();
                    }

                    ImGui::SameLine();

                    char label[64];
                    snprintf(label, sizeof(label), "Palette %02d", static_cast<int>(pi));
                    if (ImGui::Selectable(label, isPreviewed, ImGuiSelectableFlags_AllowDoubleClick)) {
                        paletteImportPreviewGroupIndex = static_cast<int>(gi);
                        paletteImportPreviewPaletteIndex = static_cast<int>(pi);
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            selections[pi] = selections[pi] == 0 ? 1 : 0;
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        paletteImportPreviewGroupIndex = static_cast<int>(gi);
                        paletteImportPreviewPaletteIndex = static_cast<int>(pi);
                    }

                    ImGui::SameLine();
                    ImVec2 swatchStart = ImGui::GetCursorScreenPos();
                    float swatchSize = ImGui::GetTextLineHeight();
                    float swatchSpacing = 2.0f;
                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    if (isPreviewed) {
                        ImVec2 previewBorderMin(swatchStart.x - 3.0f, swatchStart.y - 2.0f);
                        ImVec2 previewBorderMax(
                            swatchStart.x + 16.0f * (swatchSize + swatchSpacing) - swatchSpacing + 3.0f,
                            swatchStart.y + swatchSize + 2.0f);
                        drawList->AddRectFilled(previewBorderMin, previewBorderMax, IM_COL32(90, 130, 200, 38));
                        drawList->AddRect(previewBorderMin, previewBorderMax, IM_COL32(120, 170, 255, 170));
                    }

                    for (int ci = 0; ci < 16; ++ci) {
                        const SDL_Color& color = group.palettes[pi].colors[ci];
                        ImVec2 p0(swatchStart.x + ci * (swatchSize + swatchSpacing), swatchStart.y);
                        ImVec2 p1(p0.x + swatchSize, p0.y + swatchSize);
                        drawList->AddRectFilled(p0, p1, IM_COL32(color.r, color.g, color.b, 255));
                        drawList->AddRect(p0, p1, IM_COL32(45, 45, 45, 255));
                    }

                    ImGui::Dummy(ImVec2(16.0f * (swatchSize + swatchSpacing), swatchSize));
                    if (ImGui::IsItemHovered()) {
                        paletteImportPreviewGroupIndex = static_cast<int>(gi);
                        paletteImportPreviewPaletteIndex = static_cast<int>(pi);
                    }

                    ImGui::PopID();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("PaletteImportPreviewPane", ImVec2(0, listHeight), ImGuiChildFlags_Borders);
        const auto& previewGroup = parsedPaletteGroups[paletteImportPreviewGroupIndex];
        const Palette& previewPalette = previewGroup.palettes[paletteImportPreviewPaletteIndex];
        bool previewSelected = paletteImportSelections[paletteImportPreviewGroupIndex][paletteImportPreviewPaletteIndex] != 0;

        ImGui::TextColored(ImVec4(0.70f, 0.80f, 0.95f, 1.0f), "%s", previewGroup.name.c_str());
        ImGui::Text("Palette %02d", paletteImportPreviewPaletteIndex);
        ImGui::SameLine();
        ImGui::TextDisabled(previewSelected ? "Selected" : "Not selected");
        ImGui::Spacing();

        float previewCellSize = getScaledSize(26.0f);
        float previewSpacing = getScaledSize(5.0f);
        ImDrawList* previewDrawList = ImGui::GetWindowDrawList();
        ImVec2 previewOrigin = ImGui::GetCursorScreenPos();

        for (int ci = 0; ci < 16; ++ci) {
            int col = ci % 8;
            int row = ci / 8;
            const SDL_Color& color = previewPalette.colors[ci];
            ImVec2 p0(
                previewOrigin.x + col * (previewCellSize + previewSpacing),
                previewOrigin.y + row * (previewCellSize + previewSpacing));
            ImVec2 p1(p0.x + previewCellSize, p0.y + previewCellSize);
            previewDrawList->AddRectFilled(p0, p1, IM_COL32(color.r, color.g, color.b, 255));
            previewDrawList->AddRect(p0, p1, IM_COL32(40, 40, 40, 255));
        }

        ImGui::Dummy(ImVec2(
            8.0f * (previewCellSize + previewSpacing),
            2.0f * (previewCellSize + previewSpacing)));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int ci = 0; ci < 16; ++ci) {
            const SDL_Color& color = previewPalette.colors[ci];
            ImGui::Text(
                "%02d  #%02X%02X%02X",
                ci,
                static_cast<unsigned int>(color.r),
                static_cast<unsigned int>(color.g),
                static_cast<unsigned int>(color.b));
            if ((ci % 2) == 0) {
                ImGui::SameLine(getScaledSize(150.0f));
            }
        }

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto applyImportedPalettes = [this, &clearPaletteImportState](const std::vector<Palette>& newPalettes, const char* actionName) {
            std::vector<Palette> oldPalettes = palettes;
            int oldCurrentPalette = currentPalette;
            int oldSelectedPaletteRow = selectedPaletteRow;
            int newCurrentPalette = newPalettes.empty() ? 0 :
                SDL_clamp(currentPalette, 0, static_cast<int>(newPalettes.size()) - 1);
            int newSelectedPaletteRow = newPalettes.empty() ? -1 :
                SDL_clamp(selectedPaletteRow, -1, static_cast<int>(newPalettes.size()) - 1);

            undoManager.execute(std::make_unique<LambdaAction>(
                actionName,
                [this, newPalettes, newCurrentPalette, newSelectedPaletteRow]() {
                    this->palettes = newPalettes;
                    this->currentPalette = newCurrentPalette;
                    this->selectedPaletteRow = newSelectedPaletteRow;
                },
                [this, oldPalettes, oldCurrentPalette, oldSelectedPaletteRow]() {
                    this->palettes = oldPalettes;
                    this->currentPalette = oldCurrentPalette;
                    this->selectedPaletteRow = oldSelectedPaletteRow;
                }
            ));

            clearPaletteImportState();
            ImGui::CloseCurrentPopup();
        };

        if (!canReplace) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(ICON_FA_FILE_IMPORT " Replace All", getScaledButtonSize(140, 0))) {
            applyImportedPalettes(selectedPalettes, "Replace Palettes");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (selectedCount <= 0) {
                ImGui::SetTooltip("Select at least one palette to replace the current set.");
            }
            else {
                ImGui::SetTooltip("Replace all currently loaded palettes with the selected import set.");
            }
        }
        if (!canReplace) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (!canAppend) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(ICON_FA_PLUS " Append", getScaledButtonSize(120, 0))) {
            std::vector<Palette> appendedPalettes = palettes;
            appendedPalettes.insert(appendedPalettes.end(), selectedPalettes.begin(), selectedPalettes.end());
            applyImportedPalettes(appendedPalettes, "Append Palettes");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (selectedCount <= 0) {
                ImGui::SetTooltip("Select at least one palette to append.");
            }
            else {
                ImGui::SetTooltip("Append the selected palettes after the currently loaded ones.");
            }
        }
        if (!canAppend) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK " Cancel", getScaledButtonSize(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            clearPaletteImportState();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else if (!keepOpen) {
        clearPaletteImportState();
    }

    if (!showPaletteImportPopup && !ImGui::IsPopupOpen("Import Palettes")) {
        clearPaletteImportState();
    }
}

void Sofanthiel::clearRomAnimationImportState()
{
    romAnimationImport = RomAnimationImportState();
}

void Sofanthiel::beginRomAnimationImport(const std::string& romPath)
{
    std::vector<uint8_t> romData;
    if (!loadBinaryFile(romPath, romData)) {
        SDL_Log("Failed to open ROM file for import: %s", romPath.c_str());
        return;
    }

    clearRomAnimationImportState();
    romAnimationImport.romPath = romPath;
    romAnimationImport.romData = std::move(romData);
    romAnimationImport.showPopup = true;
    romAnimationImport.popupPendingOpen = true;
    romAnimationImport.errorMessage = "Enter a ROM offset such as 0x123456 or 08123456.";
}

void Sofanthiel::refreshRomAnimationImportPreview()
{
    romAnimationImport.previewValid = false;
    romAnimationImport.resolvedAnimationPointer = 0;
    romAnimationImport.previewAnimation = Animation();
    romAnimationImport.previewCels.clear();
    romAnimationImport.previewEntryPointers.clear();
    romAnimationImport.previewCelPointers.clear();
    romAnimationImport.previewCurrentFrame = 0;
    romAnimationImport.previewTotalFrames = 0;
    romAnimationImport.previewLastTickMs = 0;
    romAnimationImport.warningMessage.clear();

    if (romAnimationImport.romData.empty()) {
        romAnimationImport.errorMessage = "Choose a ROM file first.";
        return;
    }

    std::string pointerError;
    uint32_t animationPointer = 0;
    if (!tryParseRomAnimationPointerInput(
        romAnimationImport.offsetBuffer,
        romAnimationImport.romData.size(),
        animationPointer,
        pointerError)) {
        romAnimationImport.errorMessage = pointerError;
        return;
    }

    std::string newSuggestedAnimationName = buildRomImportSuggestedAnimationName(
        romAnimationImport.romPath,
        animationPointer);
    std::string newSuggestedCelPrefix = buildRomImportSuggestedCelPrefix(
        romAnimationImport.romPath,
        animationPointer);

    if (shouldUpdateSuggestedBuffer(
        romAnimationImport.animationNameBuffer,
        romAnimationImport.suggestedAnimationName)) {
        copyStringToBuffer(
            romAnimationImport.animationNameBuffer,
            sizeof(romAnimationImport.animationNameBuffer),
            newSuggestedAnimationName);
    }

    if (shouldUpdateSuggestedBuffer(
        romAnimationImport.celPrefixBuffer,
        romAnimationImport.suggestedCelPrefix)) {
        copyStringToBuffer(
            romAnimationImport.celPrefixBuffer,
            sizeof(romAnimationImport.celPrefixBuffer),
            newSuggestedCelPrefix);
    }

    romAnimationImport.suggestedAnimationName = newSuggestedAnimationName;
    romAnimationImport.suggestedCelPrefix = newSuggestedCelPrefix;
    romAnimationImport.resolvedAnimationPointer = animationPointer;

    std::string animationName = trimString(romAnimationImport.animationNameBuffer);
    std::string celPrefix = trimString(romAnimationImport.celPrefixBuffer);

    if (animationName.empty()) {
        romAnimationImport.errorMessage = "Animation name cannot be empty.";
        return;
    }

    if (celPrefix.empty()) {
        romAnimationImport.errorMessage = "Cel name prefix cannot be empty.";
        return;
    }

    RomAnimationImportParseResult parseResult = parseRomAnimationData(
        romAnimationImport.romData,
        animationPointer,
        animationName,
        celPrefix);

    romAnimationImport.errorMessage = parseResult.errorMessage;
    romAnimationImport.warningMessage = parseResult.warningMessage;

    if (!parseResult.success) {
        return;
    }

    romAnimationImport.previewValid = true;
    romAnimationImport.previewAnimation = std::move(parseResult.animation);
    romAnimationImport.previewCels = std::move(parseResult.cels);
    romAnimationImport.previewEntryPointers = std::move(parseResult.entryPointers);
    romAnimationImport.previewCelPointers = std::move(parseResult.celPointers);
    romAnimationImport.previewTotalFrames = calculateAnimationTotalFrames(romAnimationImport.previewAnimation);
    romAnimationImport.previewLastTickMs = SDL_GetTicks();
}

void Sofanthiel::handleRomAnimationImportPopup()
{
    auto closePopup = [this]() {
        clearRomAnimationImportState();
        ImGui::CloseCurrentPopup();
    };

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    ImVec2 minWindowSize(getScaledSize(760.0f), getScaledSize(480.0f));
    ImVec2 maxWindowSize(viewport->WorkSize.x * 0.96f, viewport->WorkSize.y * 0.92f);
    ImVec2 desiredSize(
        ImClamp(viewport->WorkSize.x * 0.78f, minWindowSize.x, getScaledSize(1040.0f)),
        ImClamp(viewport->WorkSize.y * 0.82f, minWindowSize.y, getScaledSize(760.0f)));

    if (romAnimationImport.popupPendingOpen) {
        ImGui::OpenPopup("Import Animation From ROM");
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(desiredSize, ImGuiCond_Appearing);
        romAnimationImport.popupPendingOpen = false;
    }

    ImGui::SetNextWindowSizeConstraints(minWindowSize, maxWindowSize);

    if (romAnimationImport.showPopup &&
        (romAnimationImport.romPath.empty() || romAnimationImport.romData.empty())) {
        clearRomAnimationImportState();
        return;
    }

    bool keepOpen = romAnimationImport.showPopup;
    if (ImGui::BeginPopupModal("Import Animation From ROM", &keepOpen, ImGuiWindowFlags_NoSavedSettings)) {
        romAnimationImport.showPopup = keepOpen;

        bool refreshPreview = false;

        ImGui::TextWrapped("Selected ROM: %s", romAnimationImport.romPath.c_str());
        ImGui::Spacing();

        ImGui::SetNextItemWidth(getScaledSize(220.0f));
        if (ImGui::InputText("ROM Offset", romAnimationImport.offsetBuffer, sizeof(romAnimationImport.offsetBuffer))) {
            refreshPreview = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Accepts either 0xXXXXXX ROM offsets or 08XXXXXX GBA ROM pointers.");
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh Preview")) {
            refreshPreview = true;
        }

        if (romAnimationImport.resolvedAnimationPointer != 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("Resolved: 0x%s", formatGbaPointer(romAnimationImport.resolvedAnimationPointer).c_str());
        }

        ImGui::SetNextItemWidth(getScaledSize(320.0f));
        if (ImGui::InputText(
            "Animation Name",
            romAnimationImport.animationNameBuffer,
            sizeof(romAnimationImport.animationNameBuffer))) {
            refreshPreview = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Use Placeholder##RomAnimName")) {
            copyStringToBuffer(
                romAnimationImport.animationNameBuffer,
                sizeof(romAnimationImport.animationNameBuffer),
                romAnimationImport.suggestedAnimationName);
            refreshPreview = true;
        }

        ImGui::SetNextItemWidth(getScaledSize(320.0f));
        if (ImGui::InputText(
            "Cel Name Prefix",
            romAnimationImport.celPrefixBuffer,
            sizeof(romAnimationImport.celPrefixBuffer))) {
            refreshPreview = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Use Placeholder##RomCelPrefix")) {
            copyStringToBuffer(
                romAnimationImport.celPrefixBuffer,
                sizeof(romAnimationImport.celPrefixBuffer),
                romAnimationImport.suggestedCelPrefix);
            refreshPreview = true;
        }

        if (refreshPreview) {
            refreshRomAnimationImportPreview();
        }

        std::string importValidationError;
        bool canImport = romAnimationImport.previewValid;

        if (romAnimationImport.previewValid) {
            if (!isAnimationNameUnique(romAnimationImport.previewAnimation.name)) {
                importValidationError = "An animation with that name already exists in the project.";
                canImport = false;
            }
            else {
                std::unordered_set<std::string> seenCelNames;
                for (const auto& cel : romAnimationImport.previewCels) {
                    if (!seenCelNames.insert(cel.name).second) {
                        importValidationError = "The cel name prefix produced duplicate cel names.";
                        canImport = false;
                        break;
                    }

                    if (!isCelNameUnique(cel.name)) {
                        importValidationError = "One or more imported cel names already exist in the project.";
                        canImport = false;
                        break;
                    }
                }
            }
        }

        if (!romAnimationImport.errorMessage.empty()) {
            if (trimString(romAnimationImport.offsetBuffer).empty()) {
                ImGui::TextDisabled("%s", romAnimationImport.errorMessage.c_str());
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", romAnimationImport.errorMessage.c_str());
            }
        }
        else if (romAnimationImport.previewValid) {
            ImGui::TextColored(
                ImVec4(0.60f, 0.82f, 0.62f, 1.0f),
                "Parsed %d entries using %d unique cels (%d total frames).",
                static_cast<int>(romAnimationImport.previewAnimation.entries.size()),
                static_cast<int>(romAnimationImport.previewCels.size()),
                romAnimationImport.previewTotalFrames);
        }
        else {
            ImGui::TextDisabled("Preview updates as soon as the ROM offset and names are valid.");
        }

        if (!romAnimationImport.warningMessage.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.38f, 1.0f), "%s", romAnimationImport.warningMessage.c_str());
        }

        if (!importValidationError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.70f, 0.35f, 1.0f), "%s", importValidationError.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f + getScaledSize(20.0f);
        float contentHeight = ImMax(getScaledSize(260.0f), ImGui::GetContentRegionAvail().y - footerHeight);
        float leftPaneWidth = ImMin(getScaledSize(430.0f), ImGui::GetContentRegionAvail().x * 0.48f);

        ImGui::BeginChild("RomImportEntriesPane", ImVec2(leftPaneWidth, contentHeight), ImGuiChildFlags_Borders);
        if (romAnimationImport.previewValid) {
            ImGui::TextColored(
                ImVec4(0.70f, 0.82f, 0.95f, 1.0f),
                "%s",
                romAnimationImport.previewAnimation.name.c_str());
            ImGui::TextDisabled(
                "%d entries  |  %d unique cels",
                static_cast<int>(romAnimationImport.previewAnimation.entries.size()),
                static_cast<int>(romAnimationImport.previewCels.size()));
            ImGui::Separator();

            for (size_t entryIdx = 0; entryIdx < romAnimationImport.previewAnimation.entries.size(); ++entryIdx) {
                const AnimationEntry& entry = romAnimationImport.previewAnimation.entries[entryIdx];
                uint32_t celPointer = entryIdx < romAnimationImport.previewEntryPointers.size()
                    ? romAnimationImport.previewEntryPointers[entryIdx]
                    : 0;
                int oamCount = 0;
                for (const auto& cel : romAnimationImport.previewCels) {
                    if (cel.name == entry.celName) {
                        oamCount = static_cast<int>(cel.oams.size());
                        break;
                    }
                }

                std::string pointerLabel = formatGbaPointer(celPointer);
                ImGui::Text(
                    "%03d  0x%s  %3d fr  %3d OAM  %s",
                    static_cast<int>(entryIdx),
                    pointerLabel.c_str(),
                    static_cast<int>(entry.duration),
                    oamCount,
                    entry.celName.c_str());
            }
        }
        else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* text = "Entry details appear here once a valid ROM animation is parsed.";
            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (avail.x - textSize.x) * 0.5f));
            ImGui::TextColored(ImVec4(0.48f, 0.48f, 0.54f, 1.0f), "%s", text);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("RomImportPreviewPane", ImVec2(0, contentHeight), ImGuiChildFlags_Borders);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (romAnimationImport.previewValid && romAnimationImport.previewTotalFrames > 0) {
            double msPerFrame = 1000.0 / std::max(1.0f, frameRate);
            Uint64 now = SDL_GetTicks();
            if (romAnimationImport.previewLastTickMs == 0) {
                romAnimationImport.previewLastTickMs = now;
            }
            while (now > romAnimationImport.previewLastTickMs + static_cast<Uint64>(msPerFrame)) {
                romAnimationImport.previewCurrentFrame =
                    (romAnimationImport.previewCurrentFrame + 1) % romAnimationImport.previewTotalFrames;
                romAnimationImport.previewLastTickMs += static_cast<Uint64>(msPerFrame);
            }
        }

        if (romAnimationImport.previewValid) {
            ImGui::Text("Preview");
            ImGui::SameLine();
            ImGui::TextDisabled(
                "Frame %d / %d",
                romAnimationImport.previewCurrentFrame,
                romAnimationImport.previewTotalFrames > 0 ? romAnimationImport.previewTotalFrames - 1 : 0);

            if (tiles.getSize() <= 0 || palettes.empty()) {
                ImGui::TextDisabled("Showing OAM bounds only. Load tiles and palettes to render sprite pixels.");
            }
            else {
                ImGui::TextDisabled("Using the currently loaded tiles and palettes for rendering.");
            }

            ImGui::Spacing();

            ImVec2 previewAvail = ImGui::GetContentRegionAvail();
            previewAvail.x = ImMax(previewAvail.x, getScaledSize(180.0f));
            previewAvail.y = ImMax(previewAvail.y, getScaledSize(180.0f));

            float previewScale = ImMin(
                (previewAvail.x - getScaledSize(12.0f)) / previewSize.x,
                (previewAvail.y - getScaledSize(12.0f)) / previewSize.y);
            previewScale = ImClamp(previewScale, 0.75f, 4.0f);

            ImVec2 canvasSize(previewSize.x * previewScale, previewSize.y * previewScale);
            ImVec2 canvasCursor = ImGui::GetCursorScreenPos();
            ImVec2 canvasOrigin(
                canvasCursor.x + ImMax(0.0f, (previewAvail.x - canvasSize.x) * 0.5f),
                canvasCursor.y + ImMax(0.0f, (previewAvail.y - canvasSize.y) * 0.5f));

            float previewBackground[4] = { 0.12f, 0.12f, 0.15f, 1.0f };
            drawBackground(drawList, canvasOrigin, canvasSize, previewBackground);
            drawList->AddRect(
                canvasOrigin,
                ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                IM_COL32(105, 105, 120, 255));
            drawAnimationFramePreview(
                drawList,
                canvasOrigin,
                previewScale,
                romAnimationImport.previewAnimation,
                romAnimationImport.previewCels,
                romAnimationImport.previewCurrentFrame,
                ImVec2(0.0f, 0.0f));
            drawGrid(drawList, canvasOrigin, canvasSize, previewScale);

            ImGui::Dummy(ImVec2(ImMax(canvasSize.x, 1.0f), ImMax(canvasSize.y, 1.0f)));
        }
        else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* text = "Preview appears here once the importer can decode the animation safely.";
            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (avail.x - textSize.x) * 0.5f));
            ImGui::TextColored(ImVec4(0.48f, 0.48f, 0.54f, 1.0f), "%s", text);
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!canImport) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(ICON_FA_FILE_IMPORT " Import", getScaledButtonSize(120, 0))) {
            std::vector<Animation> oldAnimations = animations;
            std::vector<AnimationCel> oldAnimationCels = animationCels;
            int oldCurrentAnimation = currentAnimation;
            int oldCurrentFrame = currentFrame;

            std::vector<Animation> newAnimations = animations;
            std::vector<AnimationCel> newAnimationCels = animationCels;
            newAnimations.push_back(romAnimationImport.previewAnimation);
            newAnimationCels.insert(
                newAnimationCels.end(),
                romAnimationImport.previewCels.begin(),
                romAnimationImport.previewCels.end());

            int newCurrentAnimation = static_cast<int>(newAnimations.size()) - 1;

            undoManager.execute(std::make_unique<LambdaAction>(
                "Import Animation From ROM",
                [this, newAnimations, newAnimationCels, newCurrentAnimation]() {
                    animations = newAnimations;
                    animationCels = newAnimationCels;
                    currentAnimation = newCurrentAnimation;
                    currentFrame = 0;
                    timelineSelectedEntryIndices.clear();
                    recalculateTotalFrames();
                },
                [this, oldAnimations, oldAnimationCels, oldCurrentAnimation, oldCurrentFrame]() {
                    animations = oldAnimations;
                    animationCels = oldAnimationCels;
                    currentAnimation = oldCurrentAnimation;
                    currentFrame = oldCurrentFrame;
                    timelineSelectedEntryIndices.clear();
                    recalculateTotalFrames();
                }));

            closePopup();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !canImport) {
            if (!importValidationError.empty()) {
                ImGui::SetTooltip("%s", importValidationError.c_str());
            }
            else if (!romAnimationImport.errorMessage.empty()) {
                ImGui::SetTooltip("%s", romAnimationImport.errorMessage.c_str());
            }
        }
        if (!canImport) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_XMARK " Cancel", getScaledButtonSize(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            closePopup();
        }

        ImGui::EndPopup();
    }
    else if (!keepOpen) {
        clearRomAnimationImportState();
    }

    if (!romAnimationImport.showPopup && !ImGui::IsPopupOpen("Import Animation From ROM")) {
        clearRomAnimationImportState();
    }
}

void Sofanthiel::update()
{
    float currentDisplayScale = this->getCurrentDisplayScale();
    this->applyDisplayScale(currentDisplayScale);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    syncImGuiDisplayMetrics(this->window);

    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(ImGui::GetID("Sofanthiel"), ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    
    if (!isDockingLayoutSetup) {
        this->setupDockingLayout();
        isDockingLayoutSetup = true;
    }

    this->updateWindowTitle();

    handleMenuBar();

    if (showExitConfirmation) {
        showAboutDialog = false;
        if (ImGui::IsPopupOpen("About Sofanthiel")) {
            ImGui::ClosePopupToLevel(0, true);
        }
    } else {
        handleAboutDialog();
    }

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

    handlePaletteImportPopup();
    handleRomAnimationImportPopup();

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
                this->currentProjectPath.clear();

                this->initializeDefaultPalettes();
                this->currentPalette = 0;
                this->updateWindowTitle();
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
                            beginPaletteImport(ResourceManager::parsePalettesFromCFile(outPath));
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
                if (ImGui::MenuItem("Import from ROM...")) {
                    nfdresult_t result = NFD_OpenDialog("gba,bin", nullptr, &outPath);

                    if (result == NFD_OKAY) {
                        beginRomAnimationImport(std::string(outPath));
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
                        selectedOAMIndices.push_back(insertPos + static_cast<int>(i));
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            bool canOptimizeSpritesheet = tiles.getSize() > 0 && !animationCels.empty() && !animations.empty();

            std::unordered_set<std::string> referencedCelNames;
            for (const auto& animation : animations) {
                for (const auto& entry : animation.entries) {
                    if (!entry.celName.empty()) {
                        referencedCelNames.insert(entry.celName);
                    }
                }
            }
            int unusedCelCount = 0;
            for (const auto& cel : animationCels) {
                if (referencedCelNames.find(cel.name) == referencedCelNames.end()) {
                    unusedCelCount++;
                }
            }

            if (ImGui::MenuItem("Optimize Spritesheet", nullptr, false, canOptimizeSpritesheet)) {
                Tiles optimizedTiles;
                std::vector<AnimationCel> optimizedAnimationCels;

                if (buildOptimizedSpritesheetState(optimizedTiles, optimizedAnimationCels)) {
                    Tiles oldTiles = this->tiles;
                    std::vector<AnimationCel> oldAnimationCels = this->animationCels;
                    bool oldCelEditingMode = this->celEditingMode;
                    int oldEditingCelIndex = this->editingCelIndex;
                    std::vector<int> oldSelectedOAMIndices = this->selectedOAMIndices;

                    undoManager.execute(std::make_unique<LambdaAction>(
                        "Optimize Spritesheet",
                        [this, optimizedTiles, optimizedAnimationCels]() {
                            this->tiles = optimizedTiles;
                            this->animationCels = optimizedAnimationCels;

                            if (this->editingCelIndex < 0 || this->editingCelIndex >= static_cast<int>(this->animationCels.size())) {
                                this->celEditingMode = false;
                                this->editingCelIndex = -1;
                                this->selectedOAMIndices.clear();
                            }
                        },
                        [this, oldTiles, oldAnimationCels, oldCelEditingMode, oldEditingCelIndex, oldSelectedOAMIndices]() {
                            this->tiles = oldTiles;
                            this->animationCels = oldAnimationCels;
                            this->celEditingMode = oldCelEditingMode;
                            this->editingCelIndex = oldEditingCelIndex;
                            this->selectedOAMIndices = oldSelectedOAMIndices;
                        }
                    ));
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (!canOptimizeSpritesheet) {
                    ImGui::SetTooltip("Load tiles, cels, and animations first.");
                }
                else {
                    ImGui::SetTooltip("Rebuilds spritesheet to only include used tiles.");
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
                        int size = 1 << i;
                        if (ImGui::MenuItem((std::to_string(size) + "x" + std::to_string(size)).c_str(), nullptr, gridSize == size)) {
                            gridSize = size;
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("DPI Scaling")) {
                float automaticDisplayScale = this->getAutomaticDisplayScale();
                ImGui::TextDisabled("Detected: %.2fx", automaticDisplayScale);

                if (ImGui::MenuItem("Automatic", nullptr, !useManualDPIScale)) {
                    useManualDPIScale = false;
                }

                ImGui::Separator();

                struct ScalePreset {
                    float scale;
                    const char* label;
                };

                static const ScalePreset scalePresets[] = {
                    { 1.0f, "100%" },
                    { 1.25f, "125%" },
                    { 1.5f, "150%" },
                    { 1.75f, "175%" },
                    { 2.0f, "200%" },
                    { 2.5f, "250%" }
                };

                for (const ScalePreset& preset : scalePresets) {
                    bool isSelected = useManualDPIScale && std::fabs(manualDPIScale - preset.scale) < 0.01f;
                    if (ImGui::MenuItem(preset.label, nullptr, isSelected)) {
                        useManualDPIScale = true;
                        manualDPIScale = preset.scale;
                    }
                }

                ImGui::Separator();

                float manualScale = manualDPIScale;
                ImGui::SetNextItemWidth(getScaledSize(140.0f));
                if (ImGui::SliderFloat("Manual", &manualScale, 1.0, 3.0, "%.2fx")) {
                    useManualDPIScale = true;
                    manualDPIScale = manualScale;
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

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Sofanthiel")) {
                showAboutDialog = true;
            }

            ImGui::EndMenu();
        }

        std::string buildLabel = BuildInfo::displayVersion();
        ImGui::SetCursorPosX(calculateRightAlignedPosition(buildLabel.c_str(), 0.0f));
        ImGui::TextDisabled("%s", buildLabel.c_str());
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
                selectedOAMIndices.push_back(insertPos + static_cast<int>(i));
            }
        }
    }
}

std::string GetFilenameFromPath(const std::string& path)
{
    size_t separator = path.find_last_of("\\/");
    if (separator == std::string::npos) {
        return path;
    }

    return path.substr(separator + 1);
}

std::string Sofanthiel::getProjectDisplayName() const
{
    if (this->currentProjectPath.empty()) {
        return "Untitled";
    }

    return GetFilenameFromPath(this->currentProjectPath);
}

std::string Sofanthiel::buildWindowTitle() const
{
    std::string title = getProjectDisplayName();
    title += " - ";
    title += BuildInfo::kAppName;
    title += " ";
    title += BuildInfo::displayVersion();
    return title;
}

void Sofanthiel::updateWindowTitle()
{
    if (this->window == nullptr) {
        return;
    }

    std::string newTitle = buildWindowTitle();
    if (newTitle == this->lastWindowTitle) {
        return;
    }

    SDL_SetWindowTitle(this->window, newTitle.c_str());
    this->lastWindowTitle = newTitle;
}

void Sofanthiel::handleAboutDialog()
{
    if (showAboutDialog) {
        ImGui::OpenPopup("About Sofanthiel");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    bool keepOpen = showAboutDialog;
    if (ImGui::BeginPopupModal("About Sofanthiel", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", BuildInfo::kAppName);
        ImGui::Separator();
        ImGui::Text("Version: %s", BuildInfo::kVersion);
        ImGui::Text("Channel: %s", BuildInfo::kBuildChannel);
        ImGui::Text("Commit: %s", BuildInfo::kGitCommit);
        ImGui::TextWrapped("Build date: %s", BuildInfo::kBuildDate);
        ImGui::Spacing();
        ImGui::TextWrapped("Repository: %s", BuildInfo::kRepositoryUrl);
        ImGui::TextWrapped("License: GPL-3.0");
        ImGui::Spacing();

        if (ImGui::Button("Close", getScaledButtonSize(400)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            keepOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    showAboutDialog = keepOpen;
}

bool Sofanthiel::buildOptimizedSpritesheetState(Tiles& outTiles, std::vector<AnimationCel>& outAnimationCels)
{
    outTiles = this->tiles;
    outAnimationCels = this->animationCels;

    const int originalTileCount = this->tiles.getSize();
    if (originalTileCount <= 0 || this->animationCels.empty() || this->animations.empty()) {
        return false;
    }

    std::unordered_set<std::string> referencedCelNames;
    for (const auto& animation : this->animations) {
        for (const auto& entry : animation.entries) {
            if (!entry.celName.empty()) {
                referencedCelNames.insert(entry.celName);
            }
        }
    }

    if (referencedCelNames.empty()) {
        return false;
    }

    std::vector<AnimationCel> usedAnimationCels;
    usedAnimationCels.reserve(this->animationCels.size());
    for (const auto& cel : this->animationCels) {
        if (referencedCelNames.find(cel.name) != referencedCelNames.end()) {
            usedAnimationCels.push_back(cel);
        }
    }

    if (usedAnimationCels.empty()) {
        return false;
    }

    std::vector<std::vector<int>> packedTileIndices;
    std::vector<TileData> uniqueTiles;
    std::unordered_map<std::string, int> tileKeyToUniqueIndex;

    auto makeTileKey = [](const TileData& tile) {
        std::string key;
        key.reserve(128);
        for (int py = 0; py < 8; ++py) {
            for (int px = 0; px < 8; ++px) {
                key.push_back(static_cast<char>(tile.data[py][px]));
            }
        }
        return key;
    };

    auto getUniqueTileIndex = [&makeTileKey, &uniqueTiles, &tileKeyToUniqueIndex](const TileData& tile) {
        std::string key = makeTileKey(tile);
        auto found = tileKeyToUniqueIndex.find(key);
        if (found != tileKeyToUniqueIndex.end()) {
            return found->second;
        }

        int newIndex = static_cast<int>(uniqueTiles.size());
        uniqueTiles.push_back(tile);
        tileKeyToUniqueIndex.emplace(std::move(key), newIndex);
        return newIndex;
    };

    auto ensureRows = [&packedTileIndices](int rowCount) {
        while (static_cast<int>(packedTileIndices.size()) < rowCount) {
            packedTileIndices.push_back(std::vector<int>(TILES_PER_LINE, -1));
        }
    };

    auto canPlaceBlockWithOverlap = [&packedTileIndices, &ensureRows](int row, int col, int widthTiles, int heightTiles, int rowStride, const std::vector<int>& desiredTileIndices) {
        if (col < 0 || widthTiles <= 0 || col + widthTiles > rowStride || heightTiles <= 0) {
            return false;
        }

        ensureRows(row + heightTiles);
        for (int ty = 0; ty < heightTiles; ++ty) {
            for (int tx = 0; tx < widthTiles; ++tx) {
                int desiredIndex = desiredTileIndices[static_cast<size_t>(ty * widthTiles + tx)];
                int existingIndex = packedTileIndices[static_cast<size_t>(row + ty)][static_cast<size_t>(col + tx)];
                if (existingIndex >= 0 && existingIndex != desiredIndex) {
                    return false;
                }
            }
        }
        return true;
    };

    auto placeBlock = [&packedTileIndices](int row, int col, int widthTiles, int heightTiles, const std::vector<int>& desiredTileIndices) {
        for (int ty = 0; ty < heightTiles; ++ty) {
            for (int tx = 0; tx < widthTiles; ++tx) {
                packedTileIndices[static_cast<size_t>(row + ty)][static_cast<size_t>(col + tx)] =
                    desiredTileIndices[static_cast<size_t>(ty * widthTiles + tx)];
            }
        }
    };

    for (auto& cel : usedAnimationCels) {
        for (auto& oam : cel.oams) {
            int width = 0;
            int height = 0;
            getOAMDimensions(oam.objShape, oam.objSize, width, height);

            int widthTiles = width / 8;
            int heightTiles = height / 8;
            if (widthTiles <= 0 || heightTiles <= 0) {
                continue;
            }

            const int rowStride = is8bppOAM(oam) ? (TILES_PER_LINE / 2) : TILES_PER_LINE;
            std::vector<int> desiredTileIndices(static_cast<size_t>(widthTiles * heightTiles), -1);
            for (int ty = 0; ty < heightTiles; ++ty) {
                for (int tx = 0; tx < widthTiles; ++tx) {
                    int srcTileIndex = getTileIndexForOffset(oam, tx, ty);
                    TileData tileData = {};
                    if (srcTileIndex >= 0 && srcTileIndex < originalTileCount) {
                        tileData = this->tiles.getTile(srcTileIndex);
                    }
                    desiredTileIndices[static_cast<size_t>(ty * widthTiles + tx)] = getUniqueTileIndex(tileData);
                }
            }

            int placedRow = -1;
            int placedCol = -1;
            for (int row = 0; placedRow < 0; ++row) {
                for (int col = 0; col <= rowStride - widthTiles; ++col) {
                    if (canPlaceBlockWithOverlap(row, col, widthTiles, heightTiles, rowStride, desiredTileIndices)) {
                        placedRow = row;
                        placedCol = col;
                        break;
                    }
                }
            }

            placeBlock(placedRow, placedCol, widthTiles, heightTiles, desiredTileIndices);

            const int newBaseIndex = placedRow * rowStride + placedCol;
            int newTileID = getTileIdFromBaseIndex(oam, newBaseIndex);
            oam.tileID = static_cast<uint16_t>(newTileID);
        }
    }

    int usedRowCount = static_cast<int>(packedTileIndices.size());
    while (usedRowCount > 0) {
        bool rowHasUsedTile = false;
        const auto& row = packedTileIndices[static_cast<size_t>(usedRowCount - 1)];
        for (int col = 0; col < TILES_PER_LINE; ++col) {
            if (row[static_cast<size_t>(col)] >= 0) {
                rowHasUsedTile = true;
                break;
            }
        }
        if (rowHasUsedTile) {
            break;
        }
        usedRowCount--;
    }

    Tiles rebuiltTiles;
    if (usedRowCount > 0) {
        rebuiltTiles.ensureSize(usedRowCount * TILES_PER_LINE);
        TileData emptyTile = {};

        for (int row = 0; row < usedRowCount; ++row) {
            for (int col = 0; col < TILES_PER_LINE; ++col) {
                int dstTileIndex = row * TILES_PER_LINE + col;
                int uniqueTileIndex = packedTileIndices[static_cast<size_t>(row)][static_cast<size_t>(col)];

                if (uniqueTileIndex >= 0 && uniqueTileIndex < static_cast<int>(uniqueTiles.size())) {
                    rebuiltTiles.setTile(dstTileIndex, uniqueTiles[static_cast<size_t>(uniqueTileIndex)]);
                }
                else {
                    rebuiltTiles.setTile(dstTileIndex, emptyTile);
                }
            }
        }
    }

    outTiles = rebuiltTiles;
    outAnimationCels = usedAnimationCels;
    return true;
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

float Sofanthiel::getAutomaticDisplayScale() const
{
    if (this->window == nullptr) {
        return 1.0;
    }

    float displayScale = SDL_GetWindowDisplayScale(this->window);
    if (std::isfinite(displayScale) && displayScale >= 1.0) {
        return displayScale;
    }

    int logicalW = 0, logicalH = 0;
    int pixelW = 0, pixelH = 0;
    SDL_GetWindowSize(this->window, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(this->window, &pixelW, &pixelH);

    float scaleX = (logicalW > 0) ? ((float)pixelW / (float)logicalW) : 0.0f;
    float scaleY = (logicalH > 0) ? ((float)pixelH / (float)logicalH) : 0.0f;

    if (scaleX > 0.0f && scaleY > 0.0f) {
        displayScale = (scaleX + scaleY) * 0.5f;
    } else if (scaleX > 0.0f) {
        displayScale = scaleX;
    } else if (scaleY > 0.0f) {
        displayScale = scaleY;
    } else {
        displayScale = 1.0;
    }

    if (!std::isfinite(displayScale) || displayScale < 1.0) {
        displayScale = 1.0;
    }

    return displayScale;
}

float Sofanthiel::getCurrentDisplayScale() const
{
    if (useManualDPIScale) {
        return SDL_clamp(manualDPIScale, 1.0f, 3.0f);
    }

    return this->getAutomaticDisplayScale();
}

void Sofanthiel::applyDisplayScale(float displayScale)
{
    if (this->window == nullptr || this->renderer == nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    if (!std::isfinite(displayScale)) {
        displayScale = 1.0f;
    }
    displayScale = SDL_clamp(displayScale, 1.0f, 3.0f);

    syncImGuiDisplayMetrics(this->window);

    ImGuiIO& io = ImGui::GetIO();

    bool scaleChanged = std::fabs(displayScale - this->dpiScale) > 0.01f;
    if (!io.Fonts->Fonts.empty() && !scaleChanged) {
        return;
    }

    SDL_Log("Applying display scale %.2f (previous %.2f)", displayScale, this->dpiScale);
    this->dpiScale = displayScale;

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    this->applyTheme();
    style.ScaleAllSizes(displayScale);

    io.Fonts->Clear();

    float baseFontSize = std::round(13.0f * displayScale);
    float iconFontSize = std::round((baseFontSize * 2.0f) / 3.0f);

    if (baseFontSize < 10.0f) {
        baseFontSize = 10.0f;
    }
    if (iconFontSize < 8.0f) {
        iconFontSize = 8.0f;
    }

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

    size_t iconFontDataSize = 0;
    void* iconFontData = loadFileToHeapBuffer("assets/" FONT_ICON_FILE_NAME_FAS, iconFontDataSize);
    ImFont* iconFont = nullptr;
    if (iconFontData != nullptr) {
        iconFont = io.Fonts->AddFontFromMemoryTTF(
            iconFontData,
            static_cast<int>(iconFontDataSize),
            iconFontSize,
            &icons_config,
            icons_ranges
        );

        if (iconFont == nullptr) {
            std::free(iconFontData);
        }
    }

    if (iconFont == nullptr) {
        SDL_Log("Warning: Could not load FontAwesome icons from assets/" FONT_ICON_FILE_NAME_FAS);
    }

    io.Fonts->Build();

    ImGui_ImplSDLRenderer3_DestroyDeviceObjects();
    ImGui_ImplSDLRenderer3_CreateDeviceObjects();

    ImTextureID fontTextureId = io.Fonts->TexRef.GetTexID();
    if (fontTextureId != ImTextureID_Invalid) {
        SDL_Texture* fontTexture = reinterpret_cast<SDL_Texture*>((size_t)fontTextureId);
        if (!SDL_SetTextureScaleMode(fontTexture, SDL_SCALEMODE_NEAREST)) {
            SDL_Log("Warning: Failed to set ImGui font texture scale mode: %s", SDL_GetError());
        }
    }
}

float Sofanthiel::calculateRightAlignedPosition(const char* text, float padding) {
    float textWidth = ImGui::CalcTextSize(text).x;
    return ImGui::GetWindowWidth() - textWidth - padding;
}

float Sofanthiel::calculateRightAlignedPosition(float elementWidth, float padding) {
    return ImGui::GetWindowWidth() - elementWidth - padding;
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
    this->updateWindowTitle();
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
    this->updateWindowTitle();
    SDL_Log("Loaded project from %s (%d tiles, %zu palettes, %zu cels, %zu anims)",
        path.c_str(), tiles.getSize(), palettes.size(),
        animationCels.size(), animations.size());
}
