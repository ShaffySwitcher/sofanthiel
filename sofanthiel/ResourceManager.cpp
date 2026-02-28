#include "ResourceManager.h"
#include "gif.h"
#include <climits>
#include <cctype>
#include <cmath>
#include <regex>
#include <set>

namespace {
std::vector<AnimationCel> parseAnimationCelsStream(std::istream& input, const std::string& sourceLabel)
{
    std::vector<AnimationCel> cels;

    std::string line;
    AnimationCel currentCel;
    bool readingCel = false;
    bool nextLineIsLength = false;
    int expectedOAMs = 0;
    int currentOAMs = 0;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.find("AnimationCel") != std::string::npos && line.find("[") != std::string::npos) {
            if (readingCel && currentCel.oams.size() > 0) {
                cels.push_back(currentCel);
            }

            size_t nameStart = line.find("AnimationCel") + 12;
            while (nameStart < line.length() && std::isspace(static_cast<unsigned char>(line[nameStart]))) nameStart++;

            size_t nameEnd = line.find("[]", nameStart);
            if (nameEnd != std::string::npos) {
                currentCel = AnimationCel();
                currentCel.name = line.substr(nameStart, nameEnd - nameStart);
                currentCel.oams.clear();
                readingCel = true;
                nextLineIsLength = true;
                expectedOAMs = 0;
                currentOAMs = 0;
            }
        }
        else if (readingCel && nextLineIsLength) {
            size_t lenPos = line.find_first_of("0123456789");
            if (lenPos != std::string::npos) {
                size_t lenEnd = line.find_first_not_of("0123456789", lenPos);
                if (lenEnd == std::string::npos) lenEnd = line.length();

                std::string lenStr = line.substr(lenPos, lenEnd - lenPos);
                try {
                    expectedOAMs = std::stoi(lenStr);
                }
                catch (...) {
                    expectedOAMs = 0;
                }
            }
            nextLineIsLength = false;
        }
        else if (readingCel && line.find("0x") != std::string::npos) {
            std::istringstream iss(line);
            std::string token;
            std::vector<uint16_t> values;

            while (iss >> token) {
                if (token.substr(0, 2) == "0x") {
                    try {
                        uint16_t value = static_cast<uint16_t>(std::stoul(token, nullptr, 16));
                        values.push_back(value);
                    }
                    catch (...) {
                    }
                }
            }

            if (values.size() == 3) {
                TengokuOAM tengokuOAM;
                memcpy(&tengokuOAM, values.data(), sizeof(TengokuOAM));

                currentCel.oams.push_back(tengokuOAM);
                currentOAMs++;
            }
        }
        else if (readingCel && line.find("};") != std::string::npos) {
            if (expectedOAMs > 0 && currentOAMs != expectedOAMs) {
                SDL_Log("Warning: Cel %s expected %d OAMs but got %d",
                    currentCel.name.c_str(), expectedOAMs, currentOAMs);
            }

            if (currentCel.oams.size() > 0) {
                cels.push_back(currentCel);
            }
            readingCel = false;
        }
    }

    if (readingCel && currentCel.oams.size() > 0) {
        cels.push_back(currentCel);
    }

    SDL_Log("Loaded %zu animation cels from %s", cels.size(), sourceLabel.c_str());
    return cels;
}

std::vector<Animation> parseAnimationsStream(std::istream& input, const std::string& sourceLabel)
{
    std::vector<Animation> animations;

    std::string line;
    Animation currentAnimation;
    bool readingAnim = false;

    while (std::getline(input, line)) {
        if (line.empty() || line.find("#include") != std::string::npos) {
            continue;
        }

        if (line.find("struct Animation") != std::string::npos && line.find("[") != std::string::npos) {
            if (readingAnim && !currentAnimation.entries.empty()) {
                animations.push_back(currentAnimation);
            }

            size_t nameStart = line.find("anim_") != std::string::npos ? line.find("anim_") : 0;
            size_t nameEnd = line.find("[]", nameStart);
            if (nameEnd != std::string::npos) {
                currentAnimation = Animation();
                currentAnimation.name = line.substr(nameStart, nameEnd - nameStart);
                currentAnimation.entries.clear();
                readingAnim = true;
            }
        }
        else if (readingAnim && line.find("{") != std::string::npos && line.find("}") != std::string::npos) {
            if (line.find("END_ANIMATION") != std::string::npos) {
                readingAnim = false;
                continue;
            }

            size_t celStart = line.find("{") + 1;
            size_t celEnd = line.find(",", celStart);

            if (celEnd != std::string::npos) {
                std::string celName = line.substr(celStart, celEnd - celStart);

                celName.erase(0, celName.find_first_not_of(" \t"));
                celName.erase(celName.find_last_not_of(" \t") + 1);

                size_t durationStart = celEnd + 1;
                size_t durationEnd = line.find("}", durationStart);
                if (durationEnd != std::string::npos) {
                    std::string durationStr = line.substr(durationStart, durationEnd - durationStart);

                    durationStr.erase(0, durationStr.find_first_not_of(" \t"));
                    durationStr.erase(durationStr.find_last_not_of(" \t") + 1);

                    try {
                        int duration = std::stoi(durationStr);

                        AnimationEntry entry;
                        entry.celName = celName;
                        entry.duration = duration;

                        currentAnimation.entries.push_back(entry);
                    }
                    catch (...) {
                        SDL_Log("Failed to parse duration in animation %s", currentAnimation.name.c_str());
                    }
                }
            }
        }
        else if (readingAnim && line.find("};") != std::string::npos) {
            if (!currentAnimation.entries.empty()) {
                animations.push_back(currentAnimation);
            }
            readingAnim = false;
        }
    }

    if (readingAnim && !currentAnimation.entries.empty()) {
        animations.push_back(currentAnimation);
    }

    SDL_Log("Loaded %zu animations from %s", animations.size(), sourceLabel.c_str());
    return animations;
}
}

std::vector<ParsedCPaletteGroup> ResourceManager::parsePalettesFromCFile(const std::string& path)
{
    std::vector<ParsedCPaletteGroup> groups;
    std::ifstream file(path);

    if (!file.is_open()) {
        SDL_Log("Failed to open palette C file: %s", path.c_str());
        return groups;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // i should have learned rust
    std::regex groupRegex(R"(Palette\s+(\w+)\s*\[\s*\]\s*=\s*\{([\s\S]*?)\};)");
    std::regex paletteBlockRegex(R"(\{([^{}]*)\})");
    std::regex colorRegex(R"(TO_RGB555\s*\(\s*0x([0-9A-Fa-f]{4,6})\s*\))");

    auto groupBegin = std::sregex_iterator(content.begin(), content.end(), groupRegex); // looks to the moon
    auto groupEnd = std::sregex_iterator(); // five pebbles

    for (auto git = groupBegin; git != groupEnd; ++git) {
        ParsedCPaletteGroup group;
        group.name = (*git)[1].str();
        std::string groupBody = (*git)[2].str();

        auto palBegin = std::sregex_iterator(groupBody.begin(), groupBody.end(), paletteBlockRegex);
        auto palEnd = std::sregex_iterator();

        for (auto pit = palBegin; pit != palEnd; ++pit) {
            std::string palBody = (*pit)[1].str();

            // extracts the RGB hex value from TO_RGB555
            auto colBegin = std::sregex_iterator(palBody.begin(), palBody.end(), colorRegex);
            auto colEnd = std::sregex_iterator();

            Palette palette;
            memset(&palette, 0, sizeof(Palette));
            int colorIdx = 0;

            // super parsing style
            for (auto cit = colBegin; cit != colEnd && colorIdx < 16; ++cit, ++colorIdx) {
                std::string hexStr = (*cit)[1].str();
                uint32_t rgb24 = std::stoul(hexStr, nullptr, 16);
                palette.colors[colorIdx].r = static_cast<uint8_t>((rgb24 >> 16) & 0xFF);
                palette.colors[colorIdx].g = static_cast<uint8_t>((rgb24 >> 8) & 0xFF);
                palette.colors[colorIdx].b = static_cast<uint8_t>(rgb24 & 0xFF);
                palette.colors[colorIdx].a = 255;
            }

            if (colorIdx > 0) {
                group.palettes.push_back(palette);
            }
        }

        if (!group.palettes.empty()) {
            groups.push_back(group);
            SDL_Log("Parsed palette group '%s' with %zu palettes from %s",
                group.name.c_str(), group.palettes.size(), path.c_str());
        }
    }

    SDL_Log("Parsed %zu palette groups from %s", groups.size(), path.c_str());
    return groups;
}

SDL_Texture* ResourceManager::loadTexture(SDL_Renderer* renderer, const std::string& path)
{
    // ask shaffy why he used a game engine to make a GUI application
    SDL_Texture* texture = nullptr;

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        SDL_Log("Failed to load image %s! SDL Error: %s\n", path.c_str(), SDL_GetError());
        return nullptr;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Failed to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError());
    }

    SDL_DestroySurface(surface);

    return texture;
}

std::vector<AnimationCel> ResourceManager::loadAnimationCels(const std::string& path)
{
    std::ifstream file(path);

    if (!file.is_open()) {
        SDL_Log("Failed to open animation cels file: %s", path.c_str());
        return {};
    }

    return parseAnimationCelsStream(file, path);
}

std::vector<AnimationCel> ResourceManager::loadAnimationCelsFromText(const std::string& text, const std::string& sourceLabel)
{
    std::istringstream input(text);
    return parseAnimationCelsStream(input, sourceLabel);
}

std::vector<Animation> ResourceManager::loadAnimations(const std::string& path)
{
    std::ifstream file(path);

    if (!file.is_open()) {
        SDL_Log("Failed to open animations file: %s", path.c_str());
        return {};
    }

    return parseAnimationsStream(file, path);
}

std::vector<Animation> ResourceManager::loadAnimationsFromText(const std::string& text, const std::string& sourceLabel)
{
    std::istringstream input(text);
    return parseAnimationsStream(input, sourceLabel);
}

std::vector<Palette> ResourceManager::loadPalettes(const std::string& path)
{
    std::vector<Palette> palettes;
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        SDL_Log("Failed to open palettes file: %s", path.c_str());
        return palettes;
    }

    MicrosoftPaletteHeader header;
	file.read(reinterpret_cast<char*>(&header), sizeof(MicrosoftPaletteHeader));

    // go through the colors and fill palettes in the vector
    for (int i = 0; i < header.numColors; i+=16) {
        Palette palette;
        for (int j = 0; j < 16; j++) {
            if (i + j >= header.numColors) {
                break;
			}
            SDL_Color color;
            file.read((char*)&color, sizeof(color));
            color.a = 255;
            palette.colors[j] = color;
        }
        palettes.push_back(palette);
	}

    file.close();
	SDL_Log("Loaded %zu palettes from %s", palettes.size(), path.c_str());

    return palettes;
}

Tiles ResourceManager::loadTiles(const std::string& path)
{
    Tiles tiles;
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        SDL_Log("Failed to open tiles file: %s", path.c_str());
        return tiles;
	}

    while(true) {
        std::array<uint8_t, 32> data;
        file.read(reinterpret_cast<char*>(data.data()), data.size());
        std::streamsize bytesRead = file.gcount();
        if (bytesRead == 0) break;
        // complete tile with zero to avoid a super crystaltile2 reference 
        if (bytesRead < 32) {
            for (size_t i = static_cast<size_t>(bytesRead); i < 32; i++) {
                data[i] = 0;
			}
        }
		tiles.addTile(data);
    }

    file.close();
    SDL_Log("Loaded %d tiles from %s", tiles.getSize(), path.c_str());
	return tiles;
}

Tiles ResourceManager::loadTilesFromImageAndPalette(const std::string& path, std::vector<Palette>& palettes, int currentPalette)
{
    Tiles tiles;

    SDL_Surface* originalSurface = IMG_Load(path.c_str());
    if (!originalSurface) {
        SDL_Log("Failed to load image %s! SDL_image Error: %s", path.c_str(), SDL_GetError());
        return tiles;
    }

    SDL_Surface* surface = originalSurface;
    bool needCleanup = false;

    if (originalSurface->w != 256 || originalSurface->h != 256) {
        SDL_Log("Resizing image from %dx%d to 256x256", originalSurface->w, originalSurface->h);
        surface = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            SDL_Log("Failed to create scaled surface! SDL Error: %s", SDL_GetError());
            SDL_DestroySurface(originalSurface);
            return tiles;
        }

        SDL_Rect srcRect = { 0, 0, originalSurface->w, originalSurface->h };
        SDL_Rect dstRect = { 0, 0, 256, 256 };
        SDL_BlitSurfaceScaled(originalSurface, &srcRect, surface, &dstRect, SDL_SCALEMODE_NEAREST);
        needCleanup = true;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (!rgbaSurface) {
        SDL_Log("Failed to convert surface to RGBA! SDL Error: %s", SDL_GetError());
        if (needCleanup) SDL_DestroySurface(surface);
        SDL_DestroySurface(originalSurface);
        return tiles;
    }

    std::vector<SDL_Color> allColors;
    std::unordered_map<uint32_t, int> colorToIndex;

    if (currentPalette >= 0 && currentPalette < static_cast<int>(palettes.size())) {
        const auto& palette = palettes[currentPalette];
        for (int i = 0; i < 16; ++i) {
            SDL_Color color = palette.colors[i];
            uint32_t colorKey = (color.r << 16) | (color.g << 8) | color.b;

            if (colorToIndex.find(colorKey) == colorToIndex.end()) {
                colorToIndex[colorKey] = static_cast<int>(allColors.size());
                allColors.push_back(color);
            }
        }
    }

    for (size_t paletteIdx = 0; paletteIdx < palettes.size(); ++paletteIdx) {
        if (static_cast<int>(paletteIdx) == currentPalette) continue;

        const auto& palette = palettes[paletteIdx];
        for (int i = 0; i < 16; ++i) {
            SDL_Color color = palette.colors[i];
            uint32_t colorKey = (color.r << 16) | (color.g << 8) | color.b;

            if (colorToIndex.find(colorKey) == colorToIndex.end()) {
                colorToIndex[colorKey] = static_cast<int>(allColors.size());
                allColors.push_back(color);
            }
        }
    }

    // lam*bad*s
    auto findClosestColor = [&](uint8_t r, uint8_t g, uint8_t b) -> int {
        int bestIndex = 0;
        int bestDistance = INT_MAX;

        for (size_t i = 0; i < allColors.size(); ++i) {
            const SDL_Color& paletteColor = allColors[i];
            int dr = r - paletteColor.r;
            int dg = g - paletteColor.g;
            int db = b - paletteColor.b;
            int distance = dr * dr + dg * dg + db * db;

            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = static_cast<int>(i);
            }
        }
        return bestIndex;
        };

    // dithering in MY rhythm tengoku :yum:
    std::vector<std::vector<SDL_Color>> quantizedImage(256, std::vector<SDL_Color>(256));
    std::vector<std::vector<float>> errorR(256, std::vector<float>(256, 0.0f));
    std::vector<std::vector<float>> errorG(256, std::vector<float>(256, 0.0f));
    std::vector<std::vector<float>> errorB(256, std::vector<float>(256, 0.0f));

    SDL_LockSurface(rgbaSurface);
    uint8_t* pixels = static_cast<uint8_t*>(rgbaSurface->pixels);
    int pitch = rgbaSurface->pitch;

    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            uint8_t* pixel = pixels + y * pitch + x * 4;

            float r = pixel[0] + errorR[y][x];
            float g = pixel[1] + errorG[y][x];
            float b = pixel[2] + errorB[y][x];

            r = std::max(0.0f, std::min(255.0f, r));
            g = std::max(0.0f, std::min(255.0f, g));
            b = std::max(0.0f, std::min(255.0f, b));

            int closestIndex = findClosestColor(static_cast<uint8_t>(r),
                static_cast<uint8_t>(g),
                static_cast<uint8_t>(b));
            SDL_Color closestColor = allColors[closestIndex];
            quantizedImage[y][x] = closestColor;

            float errR = r - closestColor.r;
            float errG = g - closestColor.g;
            float errB = b - closestColor.b;

            // thank you chatgpt, apparently that's called a Floyd-Steinberg dithering, the more you know
            if (x + 1 < 256) {
                errorR[y][x + 1] += errR * 7.0f / 16.0f;
                errorG[y][x + 1] += errG * 7.0f / 16.0f;
                errorB[y][x + 1] += errB * 7.0f / 16.0f;
            }
            if (y + 1 < 256) {
                if (x > 0) {
                    errorR[y + 1][x - 1] += errR * 3.0f / 16.0f;
                    errorG[y + 1][x - 1] += errG * 3.0f / 16.0f;
                    errorB[y + 1][x - 1] += errB * 3.0f / 16.0f;
                }
                errorR[y + 1][x] += errR * 5.0f / 16.0f;
                errorG[y + 1][x] += errG * 5.0f / 16.0f;
                errorB[y + 1][x] += errB * 5.0f / 16.0f;
                if (x + 1 < 256) {
                    errorR[y + 1][x + 1] += errR * 1.0f / 16.0f;
                    errorG[y + 1][x + 1] += errG * 1.0f / 16.0f;
                    errorB[y + 1][x + 1] += errB * 1.0f / 16.0f;
                }
            }
        }
    }

    SDL_UnlockSurface(rgbaSurface);

    for (int tileY = 0; tileY < 32; ++tileY) {
        for (int tileX = 0; tileX < 32;++tileX) {
            TileData tileData;

            for (int py = 0; py < 8; ++py) {
                for (int px = 0; px < 8; ++px) {
                    int imageX = tileX * 8 + px;
                    int imageY = tileY * 8 + py;

                    SDL_Color pixelColor = quantizedImage[imageY][imageX];

                    uint8_t paletteIndex = 0;

                    bool found = false;
                    if (currentPalette >= 0 && currentPalette < static_cast<int>(palettes.size())) {
                        const auto& palette = palettes[currentPalette];
                        for (int colorIdx = 0; colorIdx < 16; ++colorIdx) {
                            const SDL_Color& paletteColor = palette.colors[colorIdx];
                            if (paletteColor.r == pixelColor.r &&
                                paletteColor.g == pixelColor.g &&
                                paletteColor.b == pixelColor.b) {
                                paletteIndex = static_cast<uint8_t>(colorIdx);
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found) {
                        for (size_t paletteIdx = 0; paletteIdx < palettes.size() && !found; ++paletteIdx) {
                            for (int colorIdx = 0; colorIdx < 16; ++colorIdx) {
                                const SDL_Color& paletteColor = palettes[paletteIdx].colors[colorIdx];
                                if (paletteColor.r == pixelColor.r &&
                                    paletteColor.g == pixelColor.g &&
                                    paletteColor.b == pixelColor.b) {
                                    paletteIndex = static_cast<uint8_t>(colorIdx);
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }

                    tileData.data[py][px] = paletteIndex;
                }
            }

            std::array<uint8_t, 32> tileBytes;
            for (int i = 0; i < 32; ++i) {
                int py = i / 4;
                int px = (i % 4) * 2;
                uint8_t pixel1 = tileData.data[py][px] & 0x0F;
                uint8_t pixel2 = tileData.data[py][px + 1] & 0x0F;
                tileBytes[i] = pixel1 | (pixel2 << 4);
            }

            tiles.addTile(tileBytes);
        }
    }

    SDL_DestroySurface(rgbaSurface);
    if (needCleanup) SDL_DestroySurface(surface);
    SDL_DestroySurface(originalSurface);

    SDL_Log("Successfully converted image to %d tiles with quantized colors using palette %d", tiles.getSize(), currentPalette);
    return tiles;
}

void ResourceManager::saveAnimationCels(const std::string& path, const std::vector<AnimationCel>& cels)
{
    std::ofstream file;
    file.open(path, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        SDL_Log("Failed to open animation cels file for writing: %s", path.c_str());
        return;
    }

    // Write file header
    file << "// Generated by Sofanthiel [https://github.com/shaffyswitcher/sofanthiel]\n\n";

    for (const auto& cel : cels) {
        file << "AnimationCel " << cel.name << "[] = {\n";
        // Write OAM count
        file << "    /* Len */ " << cel.oams.size() << ",\n";
        // Write each OAM entry
        for (size_t i = 0; i < cel.oams.size(); ++i) {
            const auto& oam = cel.oams[i];
            file << "    /* " << std::setw(3) << std::setfill('0') << i << " */ ";
            const uint16_t* raw = reinterpret_cast<const uint16_t*>(&oam);
            file << "0x" << std::hex << std::setw(4) << std::setfill('0') << raw[0] << ", ";
            file << "0x" << std::hex << std::setw(4) << std::setfill('0') << raw[1] << ", ";
            file << "0x" << std::hex << std::setw(4) << std::setfill('0') << raw[2];
            file << std::dec; // Reset to decimal
            if (i + 1 < cel.oams.size())
                file << ",";
            file << "\n";
        }
        file << "};\n\n";
    }

    file.close();
    SDL_Log("Saved %zu animation cels to %s", cels.size(), path.c_str());
}

void ResourceManager::saveAnimations(const std::string& path, const std::vector<Animation>& animations, const std::string& cel_filename)
{
    std::ofstream file;
    file.open(path, std::ios::out | std::ios::trunc);

    if (!file.is_open()) {
        SDL_Log("Failed to open animation cels file for writing: %s", path.c_str());
        return;
    }

    // Write file header
    file << "// Generated by Sofanthiel [https://github.com/shaffyswitcher/sofanthiel]\n\n";
    file << "#include \"global.h\"\n";
    file << "#include \"graphics.h\"\n\n";
    file << "#include \"";
    file << cel_filename;
    file << "\"\n\n";

    for (const auto& anim : animations) {
        file << "struct Animation " << anim.name << "[] = {\n";
        for (size_t i = 0; i < anim.entries.size(); ++i) {
            const auto& entry = anim.entries[i];
            file << "    /* " << std::setw(3) << std::setfill('0') << i << " */ { "
                << entry.celName << ", " << static_cast<int>(entry.duration) << " },\n";
        }
        // Write END_ANIMATION marker
        file << "    /* " << std::setw(3) << std::setfill('0') << anim.entries.size() << " */ END_ANIMATION,\n";
        file << "};\n\n";
    }

    file.close();
    SDL_Log("Saved %zu animations to %s", animations.size(), path.c_str());
}

// i love worms
// https://worms2d.info/Palette_file
void ResourceManager::savePalettes(const std::string& path, const std::vector<Palette>& palettes)
{
    if (path.substr(path.find_last_of(".") + 1) == "pal") {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            SDL_Log("Failed to open palettes file for writing: %s", path.c_str());
            return;
        }

        // Calculate color count
        int colorCount = static_cast<int>(palettes.size() * 16);
        int dataChunkSize = 4 + 2 + 2 + colorCount * 4;
        int riffFileSize = 4 + 4 + dataChunkSize;

        // RIFF header
        file.write("RIFF", 4);
        uint32_t fileLength = riffFileSize;
        file.write(reinterpret_cast<const char*>(&fileLength), 4);
        file.write("PAL ", 4);

        // Data chunk
        file.write("data", 4);
        uint32_t chunkSize = dataChunkSize - 8;
        file.write(reinterpret_cast<const char*>(&chunkSize), 4);

        uint16_t version = 0x0300;
        uint16_t numColors = static_cast<uint16_t>(colorCount);
        file.write(reinterpret_cast<const char*>(&version), 2);
        file.write(reinterpret_cast<const char*>(&numColors), 2);

        // Colors
        for (const auto& palette : palettes) {
            for (const auto& color : palette.colors) {
                file.write(reinterpret_cast<const char*>(&color.r), 1);
                file.write(reinterpret_cast<const char*>(&color.g), 1);
                file.write(reinterpret_cast<const char*>(&color.b), 1);
                uint8_t flag = 0;
                file.write(reinterpret_cast<const char*>(&flag), 1);
            }
        }

        file.close();
        SDL_Log("Saved %zu palettes to %s", palettes.size(), path.c_str());
    }
    else {
        std::ofstream file;
        file.open(path, std::ios::out | std::ios::trunc);

        if (!file.is_open()) {
            SDL_Log("Failed to open palettes file for writing: %s", path.c_str());
            return;
        }

        file << "// Generated by Sofanthiel [https://github.com/shaffyswitcher/sofanthiel]\n\n";
        file << "#include \"global.h\"\n";
        file << "#include \"graphics.h\"\n\n";

        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string paletteName = filename.substr(0, filename.find_last_of("."));
        if (paletteName.empty()) {
            paletteName = "palette";
        }

        file << "Palette " << paletteName << "_pal[] = {\n";

        for (size_t paletteIndex = 0; paletteIndex < palettes.size(); ++paletteIndex) {
            const auto& palette = palettes[paletteIndex];

            file << "    /* PALETTE " << std::setw(2) << std::setfill('0') << paletteIndex << " */ {\n";

            for (size_t colorIndex = 0; colorIndex < 16; ++colorIndex) {
                const auto& color = palette.colors[colorIndex];

                uint32_t rgb24 = (static_cast<uint32_t>(color.r) << 16) |
                    (static_cast<uint32_t>(color.g) << 8) |
                    static_cast<uint32_t>(color.b);

                file << "        /* " << std::setw(2) << std::setfill('0') << colorIndex << " */ TO_RGB555(0x"
                    << std::hex << std::setw(6) << std::setfill('0') << rgb24 << ")";

                if (colorIndex < 15) {
                    file << ",";
                }

                file << std::dec << "\n";
            }

            file << "    }";
            if (paletteIndex < palettes.size() - 1) {
                file << ",";
            }
            file << "\n";
        }

        file << "};\n";

        file.close();
        SDL_Log("Saved %zu palettes to %s", palettes.size(), path.c_str());
    }
}

void ResourceManager::saveTiles(const std::string& path, Tiles& tiles)
{
    std::ofstream file(path, std::ios::binary);

    if (!file.is_open()) {
        SDL_Log("Failed to open tiles file for writing: %s", path.c_str());
        return;
    }

    for (int i = 0; i < tiles.getSize(); ++i) {
        TileData tileData = tiles.getTile(i);

        std::array<uint8_t, 32> tileBytes;
        for (int byteIndex = 0; byteIndex < 32; ++byteIndex) {
            int py = byteIndex / 4;
            int px = (byteIndex % 4) * 2;
            uint8_t pixel1 = tileData.data[py][px] & 0x0F;
            uint8_t pixel2 = tileData.data[py][px + 1] & 0x0F;
            tileBytes[byteIndex] = pixel1 | (pixel2 << 4);
        }

        file.write(reinterpret_cast<const char*>(tileBytes.data()), 32);
    }

    file.close();
    SDL_Log("Saved %d tiles to %s", tiles.getSize(), path.c_str());
}

void ResourceManager::saveTilesToImage(const std::string& path, Tiles& tiles, const std::vector<Palette>& palettes)
{
    if (tiles.getSize() == 0) {
        SDL_Log("No tiles to save to image");
        return;
    }

    SDL_Surface* surface = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        SDL_Log("Failed to create surface for tiles image! SDL Error: %s", SDL_GetError());
        return;
    }

    std::vector<SDL_Color> allColors;
    for (const auto& palette : palettes) {
        for (int i = 0; i < 16; ++i) {
            allColors.push_back(palette.colors[i]);
        }
    }

    SDL_LockSurface(surface);
    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;

    int tilesPerRow = 256/8;
    int tilesPerCol = 256/8;

    for (int tileIndex = 0; tileIndex < tiles.getSize() && tileIndex < (tilesPerRow * tilesPerCol); ++tileIndex) {
        TileData tileData = tiles.getTile(tileIndex);

        int tileX = tileIndex % tilesPerRow;
        int tileY = tileIndex / tilesPerRow;

        for (int py = 0; py < 8; ++py) {
            for (int px = 0; px < 8; ++px) {
                int imageX = tileX * 8 + px;
                int imageY = tileY * 8 + py;

                uint8_t paletteIndex = tileData.data[py][px] & 0x0F;

                SDL_Color pixelColor = { 0, 0, 0, 255 };
                if (!palettes.empty() && paletteIndex < 16) {
                    pixelColor = palettes[0].colors[paletteIndex];
                }

                uint8_t* pixel = pixels + imageY * pitch + imageX * 4;
                pixel[0] = pixelColor.r;
                pixel[1] = pixelColor.g;
                pixel[2] = pixelColor.b;
                pixel[3] = pixelColor.a;
            }
        }
    }

    SDL_UnlockSurface(surface);

    std::string extension = path.substr(path.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    bool success = false;
    if (extension == "bmp") {
        success = SDL_SaveBMP(surface, path.c_str());
    }
    else if (extension == "png") {
        success = IMG_SavePNG(surface, path.c_str());
    }
    else {
        std::string bmpPath = path.substr(0, path.find_last_of(".")) + ".bmp";
        success = SDL_SaveBMP(surface, bmpPath.c_str());
        SDL_Log("Unknown image format, saved as BMP: %s", bmpPath.c_str());
    }

    if (!success) {
        SDL_Log("Failed to save tiles image to %s! SDL Error: %s", path.c_str(), SDL_GetError());
    }
    else {
        SDL_Log("Successfully saved %d tiles to image %s", tiles.getSize(), path.c_str());
    }

    SDL_DestroySurface(surface);
}

bool ResourceManager::exportSelectionToImage(const std::string& path, Tiles& tiles,
    const std::vector<Palette>& palettes, int paletteIndex,
    int tileStartX, int tileStartY, int tileCountX, int tileCountY)
{
    if (tiles.getSize() == 0 || palettes.empty()) {
        SDL_Log("No tiles or palettes available for export");
        return false;
    }

    int pixelWidth = tileCountX * 8;
    int pixelHeight = tileCountY * 8;

    SDL_Surface* surface = SDL_CreateSurface(pixelWidth, pixelHeight, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        SDL_Log("Failed to create surface for selection export! SDL Error: %s", SDL_GetError());
        return false;
    }

    int safePalette = SDL_clamp(paletteIndex, 0, static_cast<int>(palettes.size()) - 1);

    SDL_LockSurface(surface);
    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;

    for (int ty = 0; ty < tileCountY; ++ty) {
        for (int tx = 0; tx < tileCountX; ++tx) {
            int tileIndex = (tileStartY + ty) * TILES_PER_LINE + (tileStartX + tx);
            if (tileIndex < 0 || tileIndex >= tiles.getSize()) continue;

            TileData tileData = tiles.getTile(tileIndex);

            for (int py = 0; py < 8; ++py) {
                for (int px = 0; px < 8; ++px) {
                    int imageX = tx * 8 + px;
                    int imageY = ty * 8 + py;

                    uint8_t colorIdx = tileData.data[py][px] & 0x0F;
                    SDL_Color color = palettes[safePalette].colors[colorIdx];

                    uint8_t* pixel = pixels + imageY * pitch + imageX * 4;
                    pixel[0] = color.r;
                    pixel[1] = color.g;
                    pixel[2] = color.b;
                    pixel[3] = (colorIdx == 0) ? 0 : 255; // Transparent for index 0
                }
            }
        }
    }

    SDL_UnlockSurface(surface);

    std::string extension = path.substr(path.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    bool success = false;
    if (extension == "png") {
        success = IMG_SavePNG(surface, path.c_str());
    }
    else if (extension == "bmp") {
        success = SDL_SaveBMP(surface, path.c_str());
    }
    else {
        success = IMG_SavePNG(surface, path.c_str());
    }

    if (!success) {
        SDL_Log("Failed to export selection to %s! SDL Error: %s", path.c_str(), SDL_GetError());
    }
    else {
        SDL_Log("Exported %dx%d tile selection to %s", tileCountX, tileCountY, path.c_str());
    }

    SDL_DestroySurface(surface);
    return success;
}

bool ResourceManager::importImageAtPosition(const std::string& path, Tiles& tiles,
    std::vector<Palette>& palettes, int paletteIndex,
    int tileStartX, int tileStartY)
{
    SDL_Surface* originalSurface = IMG_Load(path.c_str());
    if (!originalSurface) {
        SDL_Log("Failed to load image %s! SDL Error: %s", path.c_str(), SDL_GetError());
        return false;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(originalSurface, SDL_PIXELFORMAT_RGBA32);
    if (!rgbaSurface) {
        SDL_Log("Failed to convert surface! SDL Error: %s", SDL_GetError());
        SDL_DestroySurface(originalSurface);
        return false;
    }

    int imgW = rgbaSurface->w;
    int imgH = rgbaSurface->h;

    int tileCountX = (imgW + 7) / 8;
    int tileCountY = (imgH + 7) / 8;

    int safePalette = SDL_clamp(paletteIndex, 0, static_cast<int>(palettes.size()) - 1);

    int maxTileIndex = (tileStartY + tileCountY - 1) * TILES_PER_LINE + (tileStartX + tileCountX - 1);
    tiles.ensureSize(maxTileIndex + 1);

    SDL_LockSurface(rgbaSurface);
    uint8_t* pixels = static_cast<uint8_t*>(rgbaSurface->pixels);
    int pitch = rgbaSurface->pitch;

    const Palette& pal = palettes[safePalette];

    for (int ty = 0; ty < tileCountY; ++ty) {
        for (int tx = 0; tx < tileCountX; ++tx) {
            int tileIndex = (tileStartY + ty) * TILES_PER_LINE + (tileStartX + tx);

            TileData tileData;
            memset(&tileData, 0, sizeof(TileData));

            for (int py = 0; py < 8; ++py) {
                for (int px = 0; px < 8; ++px) {
                    int imageX = tx * 8 + px;
                    int imageY = ty * 8 + py;

                    if (imageX >= imgW || imageY >= imgH) {
                        tileData.data[py][px] = 0;
                        continue;
                    }

                    uint8_t* pixel = pixels + imageY * pitch + imageX * 4;
                    uint8_t r = pixel[0];
                    uint8_t g = pixel[1];
                    uint8_t b = pixel[2];
                    uint8_t a = pixel[3];

                    if (a < 128) {
                        tileData.data[py][px] = 0;
                        continue;
                    }

                    int bestIdx = 0;
                    int bestDist = INT_MAX;
                    for (int ci = 0; ci < 16; ++ci) {
                        int dr = r - pal.colors[ci].r;
                        int dg = g - pal.colors[ci].g;
                        int db = b - pal.colors[ci].b;
                        int dist = dr * dr + dg * dg + db * db;
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = ci;
                        }
                    }
                    tileData.data[py][px] = static_cast<uint8_t>(bestIdx);
                }
            }

            tiles.setTile(tileIndex, tileData);
        }
    }

    SDL_UnlockSurface(rgbaSurface);
    SDL_DestroySurface(rgbaSurface);
    SDL_DestroySurface(originalSurface);

    SDL_Log("Imported %dx%d image at tile position (%d, %d)", imgW, imgH, tileStartX, tileStartY);
    return true;
}

bool ResourceManager::convertImageToSpritesheetAndPalette(const std::string& path,
    Tiles& outTiles, std::vector<Palette>& outPalettes)
{
    SDL_Surface* originalSurface = IMG_Load(path.c_str());
    if (!originalSurface) {
        SDL_Log("Failed to load image %s! SDL Error: %s", path.c_str(), SDL_GetError());
        return false;
    }

    SDL_Surface* surface = originalSurface;
    bool needCleanup = false;
    if (originalSurface->w != 256 || originalSurface->h != 256) {
        SDL_Log("Resizing image from %dx%d to 256x256", originalSurface->w, originalSurface->h);
        surface = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            SDL_DestroySurface(originalSurface);
            return false;
        }
        SDL_Rect srcRect = { 0, 0, originalSurface->w, originalSurface->h };
        SDL_Rect dstRect = { 0, 0, 256, 256 };
        SDL_BlitSurfaceScaled(originalSurface, &srcRect, surface, &dstRect, SDL_SCALEMODE_NEAREST);
        needCleanup = true;
    }

    SDL_Surface* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (!rgbaSurface) {
        if (needCleanup) SDL_DestroySurface(surface);
        SDL_DestroySurface(originalSurface);
        return false;
    }

    SDL_LockSurface(rgbaSurface);
    uint8_t* pixels = static_cast<uint8_t*>(rgbaSurface->pixels);
    int pitch = rgbaSurface->pitch;

    struct ColorEntry { uint8_t r, g, b; };
    std::unordered_map<uint32_t, int> colorCounts;

    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            uint8_t* px = pixels + y * pitch + x * 4;
            uint32_t key = (px[0] << 16) | (px[1] << 8) | px[2];
            colorCounts[key]++;
        }
    }

    std::vector<std::pair<uint32_t, int>> colorList(colorCounts.begin(), colorCounts.end());
    std::sort(colorList.begin(), colorList.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    Palette extractedPalette;
    memset(&extractedPalette, 0, sizeof(Palette));
    extractedPalette.colors[0] = { 0, 0, 0, 0 };

    int numColors = std::min(15, static_cast<int>(colorList.size()));

    if (numColors <= 15) {
        for (int i = 0; i < numColors; ++i) {
            uint32_t key = colorList[i].first;
            extractedPalette.colors[i + 1].r = (key >> 16) & 0xFF;
            extractedPalette.colors[i + 1].g = (key >> 8) & 0xFF;
            extractedPalette.colors[i + 1].b = key & 0xFF;
            extractedPalette.colors[i + 1].a = 255;
        }
    }

    outPalettes.clear();
    outPalettes.push_back(extractedPalette);

    SDL_UnlockSurface(rgbaSurface);

    outTiles = loadTilesFromImageAndPalette(path, outPalettes, 0);

    SDL_DestroySurface(rgbaSurface);
    if (needCleanup) SDL_DestroySurface(surface);
    SDL_DestroySurface(originalSurface);

    SDL_Log("Converted image to spritesheet (%d tiles) with extracted %d-color palette",
        outTiles.getSize(), numColors + 1);
    return true;
}

static void getOAMDimensionsStatic(int objShape, int objSize, int& width, int& height)
{
    width = 8; height = 8;
    switch (objShape) {
    case 0: // SQUARE
        switch (objSize) {
        case 0: width = 8; height = 8; break;
        case 1: width = 16; height = 16; break;
        case 2: width = 32; height = 32; break;
        case 3: width = 64; height = 64; break;
        } break;
    case 1: // HORIZONTAL
        switch (objSize) {
        case 0: width = 16; height = 8; break;
        case 1: width = 32; height = 8; break;
        case 2: width = 32; height = 16; break;
        case 3: width = 64; height = 32; break;
        } break;
    case 2: // VERTICAL
        switch (objSize) {
        case 0: width = 8; height = 16; break;
        case 1: width = 8; height = 32; break;
        case 2: width = 16; height = 32; break;
        case 3: width = 32; height = 64; break;
        } break;
    }
}

bool ResourceManager::exportAnimationToGif(const std::string& path,
    const std::vector<Animation>& animations, int animIndex,
    const std::vector<AnimationCel>& cels,
    Tiles& tiles, const std::vector<Palette>& palettes,
    float frameRate, int width, int height,
    float offsetX, float offsetY, int scale)
{
    if (animIndex < 0 || animIndex >= static_cast<int>(animations.size())) {
        SDL_Log("Invalid animation index %d for GIF export", animIndex);
        return false;
    }

    const Animation& anim = animations[animIndex];
    if (anim.entries.empty()) {
        SDL_Log("Animation '%s' has no entries", anim.name.c_str());
        return false;
    }

    int bboxMinX = width, bboxMinY = height, bboxMaxX = 0, bboxMaxY = 0;

    for (const auto& entry : anim.entries) {
        if (entry.duration == 0) continue;
        const AnimationCel* cel = nullptr;
        for (const auto& c : cels) {
            if (c.name == entry.celName) { cel = &c; break; }
        }
        if (!cel) continue;

        for (const auto& oam : cel->oams) {
            int oamW = 0, oamH = 0;
            getOAMDimensionsStatic(oam.objShape, oam.objSize, oamW, oamH);

            int x = static_cast<int>(oam.xPosition + offsetX);
            int y = static_cast<int>(oam.yPosition + offsetY);

            if (x < bboxMinX) bboxMinX = x;
            if (y < bboxMinY) bboxMinY = y;
            if (x + oamW > bboxMaxX) bboxMaxX = x + oamW;
            if (y + oamH > bboxMaxY) bboxMaxY = y + oamH;
        }
    }

    if (bboxMinX < 0) bboxMinX = 0;
    if (bboxMinY < 0) bboxMinY = 0;
    if (bboxMaxX > width) bboxMaxX = width;
    if (bboxMaxY > height) bboxMaxY = height;

    if (bboxMaxX <= bboxMinX || bboxMaxY <= bboxMinY) {
        SDL_Log("No visible content in animation for GIF export");
        return false;
    }

    int cropW = (bboxMaxX - bboxMinX) * scale;
    int cropH = (bboxMaxY - bboxMinY) * scale;

    // fuck compression
    std::set<uint32_t> usedColors;
    for (const auto& entry : anim.entries) {
        const AnimationCel* cel = nullptr;
        for (const auto& c : cels) {
            if (c.name == entry.celName) { cel = &c; break; }
        }
        if (!cel) continue;
        for (const auto& oam : cel->oams) {
            int palIdx = oam.palette;
            if (palIdx >= static_cast<int>(palettes.size())) continue;
            for (int ci = 1; ci < 16; ci++) {
                const SDL_Color& clr = palettes[palIdx].colors[ci];
                usedColors.insert(((uint32_t)clr.r << 16) | ((uint32_t)clr.g << 8) | clr.b);
            }
        }
    }

    std::vector<uint32_t> paletteColors;
    paletteColors.push_back(0); // transparency
    std::unordered_map<uint32_t, uint8_t> colorToIndex;
    for (uint32_t c : usedColors) {
        if (paletteColors.size() >= 255) break;
        colorToIndex[c] = static_cast<uint8_t>(paletteColors.size());
        paletteColors.push_back(c);
    }

    int numColors = static_cast<int>(paletteColors.size());
    int bitDepth = 2;
    while ((1 << bitDepth) < numColors && bitDepth < 8) bitDepth++;

    GifPalette gifPal = {};
    gifPal.bitDepth = bitDepth;
    for (size_t i = 1; i < paletteColors.size() && i < 256; i++) {
        uint32_t c = paletteColors[i];
        gifPal.r[i] = (c >> 16) & 0xFF;
        gifPal.g[i] = (c >> 8) & 0xFF;
        gifPal.b[i] = c & 0xFF;
    }

    const double exportFrameRate = (frameRate > 0.0f) ? static_cast<double>(frameRate) : 60.0;

    uint32_t defaultDelay = static_cast<uint32_t>(std::ceil(100.0 / exportFrameRate));
    if (defaultDelay < 2) defaultDelay = 2;

    GifWriter writer = {};
    if (!GifBegin(&writer, path.c_str(), cropW, cropH, defaultDelay, bitDepth)) {
        SDL_Log("Failed to create GIF file: %s", path.c_str());
        return false;
    }

    std::vector<uint8_t> frameBuffer(cropW * cropH * 4, 0);

    int totalFrames = 0;
    for (const auto& entry : anim.entries)
        totalFrames += entry.duration;

    double idealTimeCentiseconds = 0.0;
    double actualTimeCentiseconds = 0.0;

    for (const auto& entry : anim.entries) {
        if (entry.duration == 0) continue;

        const AnimationCel* cel = nullptr;
        for (const auto& c : cels) {
            if (c.name == entry.celName) { cel = &c; break; }
        }

        memset(frameBuffer.data(), 0, frameBuffer.size());

        if (cel) {
            for (int i = static_cast<int>(cel->oams.size()) - 1; i >= 0; i--) {
                const TengokuOAM& oam = cel->oams[i];

                int oamW = 0, oamH = 0;
                getOAMDimensionsStatic(oam.objShape, oam.objSize, oamW, oamH);

                int paletteIndex = oam.palette;
                if (paletteIndex >= static_cast<int>(palettes.size()) || palettes.empty()) continue;

                float baseX = oam.xPosition + offsetX;
                float baseY = oam.yPosition + offsetY;

                for (int ty = 0; ty < oamH / 8; ty++) {
                    for (int tx = 0; tx < oamW / 8; tx++) {
                        int tileX = oam.hFlip ? (oamW / 8 - 1 - tx) : tx;
                        int tileY = oam.vFlip ? (oamH / 8 - 1 - ty) : ty;
                        int tileIdx = oam.tileID + tileY * 32 + tileX;

                        if (tileIdx >= tiles.getSize()) continue;

                        TileData tile = tiles.getTile(tileIdx);

                        for (int py = 0; py < 8; py++) {
                            for (int px = 0; px < 8; px++) {
                                int pixelX = oam.hFlip ? (7 - px) : px;
                                int pixelY = oam.vFlip ? (7 - py) : py;

                                uint8_t colorIdx = tile.data[pixelY][pixelX];
                                if (colorIdx == 0) continue;

                                SDL_Color color = palettes[paletteIndex].colors[colorIdx];

                                int imgX = static_cast<int>(baseX + tx * 8 + px) - bboxMinX;
                                int imgY = static_cast<int>(baseY + ty * 8 + py) - bboxMinY;

                                for (int sy = 0; sy < scale; sy++) {
                                    for (int sx = 0; sx < scale; sx++) {
                                        int finalX = imgX * scale + sx;
                                        int finalY = imgY * scale + sy;
                                        if (finalX >= 0 && finalX < cropW && finalY >= 0 && finalY < cropH) {
                                            int idx = (finalY * cropW + finalX) * 4;
                                            frameBuffer[idx + 0] = color.r;
                                            frameBuffer[idx + 1] = color.g;
                                            frameBuffer[idx + 2] = color.b;
                                            frameBuffer[idx + 3] = 255;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        writer.firstFrame = false;

        for (int p = 0; p < cropW * cropH; p++) {
            if (frameBuffer[p * 4 + 3] == 0) {
                writer.oldImage[p * 4 + 0] = 0;
                writer.oldImage[p * 4 + 1] = 0;
                writer.oldImage[p * 4 + 2] = 0;
                writer.oldImage[p * 4 + 3] = kGifTransIndex;
            } else {
                uint32_t key = ((uint32_t)frameBuffer[p * 4 + 0] << 16) |
                               ((uint32_t)frameBuffer[p * 4 + 1] << 8) |
                               (uint32_t)frameBuffer[p * 4 + 2];
                auto it = colorToIndex.find(key);
                uint8_t palIdx = (it != colorToIndex.end()) ? it->second : 1;
                writer.oldImage[p * 4 + 0] = gifPal.r[palIdx];
                writer.oldImage[p * 4 + 1] = gifPal.g[palIdx];
                writer.oldImage[p * 4 + 2] = gifPal.b[palIdx];
                writer.oldImage[p * 4 + 3] = palIdx;
            }
        }

        idealTimeCentiseconds += (static_cast<double>(entry.duration) / exportFrameRate) * 100.0;
        double delayDelta = idealTimeCentiseconds - actualTimeCentiseconds;
        uint32_t entryDelay = static_cast<uint32_t>(std::lround(delayDelta));
        if (entryDelay < 1) entryDelay = 1;
        actualTimeCentiseconds += entryDelay;

        GifWriteLzwImage(writer.f, writer.oldImage, 0, 0, cropW, cropH, entryDelay, &gifPal, 2);
    }

    GifEnd(&writer);
    SDL_Log("Exported animation '%s' (%d frames, %dx%d, %d-bit, %d colors) to GIF: %s",
        anim.name.c_str(), totalFrames, cropW, cropH, bitDepth,
        static_cast<int>(paletteColors.size()), path.c_str());
    return true;
}
