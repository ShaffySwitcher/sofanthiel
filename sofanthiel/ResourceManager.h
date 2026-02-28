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

struct ParsedCPaletteGroup {
	std::string name;
	std::vector<Palette> palettes;
};

class ResourceManager
{
public:
	static SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path);

	static std::vector<AnimationCel> loadAnimationCels(const std::string& path);
	static std::vector<AnimationCel> loadAnimationCelsFromText(const std::string& text, const std::string& sourceLabel = "<memory>");
	static std::vector<Animation> loadAnimations(const std::string& path);
	static std::vector<Animation> loadAnimationsFromText(const std::string& text, const std::string& sourceLabel = "<memory>");
	static std::vector<Palette> loadPalettes(const std::string& path);
	static std::vector<ParsedCPaletteGroup> parsePalettesFromCFile(const std::string& path);
	static Tiles loadTiles(const std::string& path);
	static Tiles loadTilesFromImageAndPalette(const std::string& path, std::vector<Palette>& palettes, int currentPalette);

	static void saveAnimationCels(const std::string& path, const std::vector<AnimationCel>& cels);
	static void saveAnimations(const std::string& path, const std::vector<Animation>& animations, const std::string& cel_filename);
	static void savePalettes(const std::string& path, const std::vector<Palette>& palettes);
	static void saveTiles(const std::string& path, Tiles& tiles);
	static void saveTilesToImage(const std::string& path, Tiles& tiles, const std::vector<Palette>& palettes);

	static bool exportSelectionToImage(const std::string& path, Tiles& tiles,
		const std::vector<Palette>& palettes, int paletteIndex,
		int tileStartX, int tileStartY, int tileCountX, int tileCountY);
	static bool importImageAtPosition(const std::string& path, Tiles& tiles,
		std::vector<Palette>& palettes, int paletteIndex,
		int tileStartX, int tileStartY);

	static bool convertImageToSpritesheetAndPalette(const std::string& path,
		Tiles& outTiles, std::vector<Palette>& outPalettes);

	static bool exportAnimationToGif(const std::string& path,
		const std::vector<Animation>& animations, int animIndex,
		const std::vector<AnimationCel>& cels,
		Tiles& tiles, const std::vector<Palette>& palettes,
		float frameRate, int width, int height,
		float offsetX, float offsetY, int scale = 1);
};

