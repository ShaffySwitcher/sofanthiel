#include "ResourceManager.h"
#include "gif.h"
#include <climits>

SDL_Texture* ResourceManager::loadTexture(SDL_Renderer* renderer, std::string path)
{
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

std::vector<AnimationCel> ResourceManager::loadAnimationCels(std::string path)
{
    std::vector<AnimationCel> cels;
    std::ifstream file(path);

    if (!file.is_open()) {
        SDL_Log("Failed to open animation cels file: %s", path.c_str());
        return cels;
    }

    std::string line;
    AnimationCel currentCel;
    bool readingCel = false;
    bool nextLineIsLength = false;
    int expectedOAMs = 0;
    int currentOAMs = 0;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.find("AnimationCel") != std::string::npos && line.find("[") != std::string::npos) {
            if (readingCel && currentCel.oams.size() > 0) {
                cels.push_back(currentCel);
            }

            size_t nameStart = line.find("AnimationCel") + 12; // "AnimationCel" length
            while (nameStart < line.length() && std::isspace(line[nameStart])) nameStart++;

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
                        uint16_t value = std::stoul(token, nullptr, 16);
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

    file.close();
    SDL_Log("Loaded %zu animation cels from %s", cels.size(), path.c_str());
    return cels;
}

std::vector<Animation> ResourceManager::loadAnimations(std::string path)
{
    std::vector<Animation> animations;
    std::ifstream file(path);

    if (!file.is_open()) {
        SDL_Log("Failed to open animations file: %s", path.c_str());
        return animations;
    }

    std::string line;
    Animation currentAnimation;
    bool readingAnim = false;

    while (std::getline(file, line)) {
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

    file.close();
    SDL_Log("Loaded %zu animations from %s", animations.size(), path.c_str());
    return animations;
}

std::vector<Palette> ResourceManager::loadPalettes(std::string path)
{
    std::vector<Palette> palettes;
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        SDL_Log("Failed to open animations file: %s", path.c_str());
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

Tiles ResourceManager::loadTiles(std::string path)
{
    Tiles tiles;
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        SDL_Log("Failed to open tiles file: %s", path.c_str());
        return tiles;
	}

	// Read 32 bytes until end of file
    while(!file.eof()) {
        std::array<uint8_t, 32> data;
        file.read(reinterpret_cast<char*>(data.data()), data.size());
        // If we read less than 32 bytes, complete with zeros
        if (file.gcount() < 32) {
            for (size_t i = file.gcount(); i < 32; i++) {
                data[i] = 0;
			}
        }
        // Add the tile to the collection
		tiles.addTile(data);
    }

    file.close();
    SDL_Log("Loaded %d tiles from %s", tiles.getSize(), path.c_str());
	return tiles;
}

Tiles ResourceManager::loadTilesFromImageAndPalette(std::string path, std::vector<Palette>& palettes, int currentPalette)
{
    Tiles tiles;

    // Load the image using SDL_image
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

            // Calculate error
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
                tileBytes[i] = (pixel1 << 4) | pixel2;
            }

            tiles.addTile(tileBytes);
        }
    }

    // Cleanup
    SDL_DestroySurface(rgbaSurface);
    if (needCleanup) SDL_DestroySurface(surface);
    SDL_DestroySurface(originalSurface);

    SDL_Log("Successfully converted image to %d tiles with quantized colors using palette %d", tiles.getSize(), currentPalette);
    return tiles;
}

void ResourceManager::saveAnimationCels(std::string path, std::vector<AnimationCel>& cels)
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

void ResourceManager::saveAnimations(std::string path, std::vector<Animation>& animations, std::string& cel_filename)
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

void ResourceManager::savePalettes(std::string path, std::vector<Palette>& palettes)
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
        // Export C files
        std::ofstream file;
        file.open(path, std::ios::out | std::ios::trunc);

        if (!file.is_open()) {
            SDL_Log("Failed to open palettes file for writing: %s", path.c_str());
            return;
        }

        // Write file header
        file << "// Generated by Sofanthiel [https://github.com/shaffyswitcher/sofanthiel]\n\n";
        file << "#include \"global.h\"\n";
        file << "#include \"graphics.h\"\n\n";

        // Generate palette name from filename
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

                // Convert RGB to 24-bit hex value
                uint32_t rgb24 = (static_cast<uint32_t>(color.r) << 16) |
                    (static_cast<uint32_t>(color.g) << 8) |
                    static_cast<uint32_t>(color.b);

                file << "        /* " << std::setw(2) << std::setfill('0') << colorIndex << " */ TO_RGB555(0x"
                    << std::hex << std::setw(6) << std::setfill('0') << rgb24 << ")";

                if (colorIndex < 15) {
                    file << ",";
                }

                file << std::dec << "\n"; // Reset to decimal
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

void ResourceManager::saveTiles(std::string path, Tiles& tiles)
{
    std::ofstream file(path, std::ios::binary);

    if (!file.is_open()) {
        SDL_Log("Failed to open tiles file for writing: %s", path.c_str());
        return;
    }

    // Write each tile as 32 bytes
    for (int i = 0; i < tiles.getSize(); ++i) {
        TileData tileData = tiles.getTile(i);

        // Convert 8x8 4-bit tile data to 32 bytes (4 bits per pixel)
        std::array<uint8_t, 32> tileBytes;
        for (int byteIndex = 0; byteIndex < 32; ++byteIndex) {
            int py = byteIndex / 4;
            int px = (byteIndex % 4) * 2;
            uint8_t pixel1 = tileData.data[py][px] & 0x0F;
            uint8_t pixel2 = tileData.data[py][px + 1] & 0x0F;
            tileBytes[byteIndex] = (pixel1 << 4) | pixel2;
        }

        file.write(reinterpret_cast<const char*>(tileBytes.data()), 32);
    }

    file.close();
    SDL_Log("Saved %d tiles to %s", tiles.getSize(), path.c_str());
}

void ResourceManager::saveTilesToImage(std::string path, Tiles& tiles, std::vector<Palette>& palettes)
{
    if (tiles.getSize() == 0) {
        SDL_Log("No tiles to save to image");
        return;
    }

    // Create a 256x256 surface (32x32 tiles of 8x8 pixels each)
    SDL_Surface* surface = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        SDL_Log("Failed to create surface for tiles image! SDL Error: %s", SDL_GetError());
        return;
    }

    // Create a combined palette from all available palettes for color lookup
    std::vector<SDL_Color> allColors;
    for (const auto& palette : palettes) {
        for (int i = 0; i < 16; ++i) {
            allColors.push_back(palette.colors[i]);
        }
    }

    SDL_LockSurface(surface);
    uint8_t* pixels = static_cast<uint8_t*>(surface->pixels);
    int pitch = surface->pitch;

    // Convert tiles back to image
    int tilesPerRow = 32; // 256 pixels / 8 pixels per tile = 32 tiles per row
    int tilesPerCol = 32; // 256 pixels / 8 pixels per tile = 32 tiles per column

    for (int tileIndex = 0; tileIndex < tiles.getSize() && tileIndex < (tilesPerRow * tilesPerCol); ++tileIndex) {
        TileData tileData = tiles.getTile(tileIndex);

        // Calculate tile position in the grid
        int tileX = tileIndex % tilesPerRow;
        int tileY = tileIndex / tilesPerRow;

        // Convert each pixel in the tile
        for (int py = 0; py < 8; ++py) {
            for (int px = 0; px < 8; ++px) {
                int imageX = tileX * 8 + px;
                int imageY = tileY * 8 + py;

                // Get palette index from tile data
                uint8_t paletteIndex = tileData.data[py][px] & 0x0F;

                // Get color from the first palette (or could be made configurable)
                SDL_Color pixelColor = { 0, 0, 0, 255 }; // Default to black
                if (!palettes.empty() && paletteIndex < 16) {
                    pixelColor = palettes[0].colors[paletteIndex];
                }

                // Set pixel in surface
                uint8_t* pixel = pixels + imageY * pitch + imageX * 4;
                pixel[0] = pixelColor.r; // R
                pixel[1] = pixelColor.g; // G
                pixel[2] = pixelColor.b; // B
                pixel[3] = pixelColor.a; // A
            }
        }
    }

    SDL_UnlockSurface(surface);

    // Determine output format from file extension and save
    std::string extension = path.substr(path.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    bool success = false;
    if (extension == "bmp") {
        success = SDL_SaveBMP(surface, path.c_str());
    }
    else if (extension == "png") {
        // For PNG, we need to use IMG_SavePNG if available
        success = IMG_SavePNG(surface, path.c_str());
    }
    else {
        // Default to BMP if unknown extension
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

