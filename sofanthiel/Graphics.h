#pragma once

#include <cinttypes>
#include <string>
#include <vector>
#include <array>

#include <SDL3/SDL.h>

#define TILES_PER_LINE 32
#define GBA_WIDTH 240
#define GBA_HEIGHT 160

struct TengokuOAM {
	// Attribute 0
	int16_t yPosition : 8;
	int16_t affineFlag : 1;
	int16_t objDisable : 1;
	int16_t objMode : 2;
	int16_t mosaicFlag : 1;
	int16_t paletteMode : 1;
	uint16_t objShape : 2;

	// Attribute 1
	int16_t xPosition : 9;
	uint16_t unused : 3;
	uint16_t hFlip : 1;
	uint16_t vFlip : 1;
	uint16_t objSize : 2;

	// Attribute 2
	uint16_t tileID : 10;
	uint16_t priority : 2;
	uint16_t palette : 4;
};

enum OAMShape : uint8_t {
	SHAPE_SQUARE = 0,
	SHAPE_HORIZONTAL = 1,
	SHAPE_VERTICAL = 2
};

struct Palette {
	SDL_Color colors[16];
};

struct AnimationEntry {
	std::string celName;
	uint8_t duration = 0;
};

struct Animation {
	std::string name;
	std::vector<AnimationEntry> entries;
};

struct AnimationCel {
	std::string name;
	std::vector<TengokuOAM> oams;
};

struct TileData {
	uint8_t data[8][8];
};

class Tiles {
public:
	void addTile(std::array<uint8_t, 32> data);

	TileData getTile(int index) const;

	int getSize() const;

	int getWidth() const;
	int getHeight() const;

	void clear();

private:
	std::vector<TileData> tiles;
};