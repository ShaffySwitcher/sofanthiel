#pragma once

#include <string>
#include <vector>
#include <iomanip>
#include <array>
#include <sstream>
#include <fstream>
#include <imgui.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "Graphics.h"
#include <unordered_map>
#include <ranges>
#include <algorithm>

struct MicrosoftPaletteHeader {
	char magic[4];
	uint32_t fileSize;
	char type[4];
	char data[4];
	uint32_t size;
	uint16_t version;
	uint16_t numColors;
};

class ResourceManager
{
public:
	static SDL_Texture* loadTexture(SDL_Renderer* renderer, std::string path);

	static std::vector<AnimationCel> loadAnimationCels(std::string path);
	static std::vector<Animation> loadAnimations(std::string path);
	static std::vector<Palette> loadPalettes(std::string path);
	static Tiles loadTiles(std::string path);
	static Tiles loadTilesFromImageAndPalette(std::string path, std::vector<Palette>& palettes, int currentPalette);

	static void saveAnimationCels(std::string path, std::vector<AnimationCel>& cels);
	static void saveAnimations(std::string path, std::vector<Animation>& animations, std::string& cel_filename);
	static void savePalettes(std::string path, std::vector<Palette>& palettes);
	static void saveTiles(std::string path, Tiles& tiles);
	static void saveTilesToImage(std::string path, Tiles& tiles, std::vector<Palette>& palettes);
};

