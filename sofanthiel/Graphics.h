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

enum OAMGfxMode : uint8_t {
	OBJ_MODE_NORMAL = 0,
	OBJ_MODE_BLEND = 1,
	OBJ_MODE_WINDOW = 2,
	OBJ_MODE_PROHIBITED = 3
};

inline bool isAffineOAM(const TengokuOAM& oam)
{
	return oam.affineFlag != 0;
}

inline bool isHiddenOAM(const TengokuOAM& oam)
{
	return !isAffineOAM(oam) && oam.objDisable != 0;
}

inline bool usesDoubleSizeOAM(const TengokuOAM& oam)
{
	return isAffineOAM(oam) && oam.objDisable != 0;
}

inline bool is8bppOAM(const TengokuOAM& oam)
{
	return oam.paletteMode != 0;
}

inline uint8_t getAffineIndex(const TengokuOAM& oam)
{
	return static_cast<uint8_t>((oam.unused & 0x07) | ((oam.hFlip & 0x01) << 3) | ((oam.vFlip & 0x01) << 4));
}

inline void setAffineIndex(TengokuOAM& oam, uint8_t index)
{
	oam.unused = static_cast<uint16_t>(index & 0x07);
	oam.hFlip = static_cast<uint16_t>((index >> 3) & 0x01);
	oam.vFlip = static_cast<uint16_t>((index >> 4) & 0x01);
}

inline int getOAMTilesWide(const TengokuOAM& oam)
{
	int width = 0;
	switch (oam.objShape) {
	case SHAPE_SQUARE:
		width = (1 << oam.objSize) * 8;
		break;
	case SHAPE_HORIZONTAL:
		width = (oam.objSize == 0) ? 16 : (oam.objSize == 1) ? 32 : (oam.objSize == 2) ? 32 : 64;
		break;
	case SHAPE_VERTICAL:
		width = (oam.objSize == 0) ? 8 : (oam.objSize == 1) ? 8 : (oam.objSize == 2) ? 16 : 32;
		break;
	default:
		width = 8;
		break;
	}
	return width / 8;
}

inline int getOAMTilesHigh(const TengokuOAM& oam)
{
	int height = 0;
	switch (oam.objShape) {
	case SHAPE_SQUARE:
		height = (1 << oam.objSize) * 8;
		break;
	case SHAPE_HORIZONTAL:
		height = (oam.objSize == 0) ? 8 : (oam.objSize == 1) ? 8 : (oam.objSize == 2) ? 16 : 32;
		break;
	case SHAPE_VERTICAL:
		height = (oam.objSize == 0) ? 16 : (oam.objSize == 1) ? 32 : (oam.objSize == 2) ? 32 : 64;
		break;
	default:
		height = 8;
		break;
	}
	return height / 8;
}

inline int getTileBaseIndex(const TengokuOAM& oam)
{
	return is8bppOAM(oam) ? (oam.tileID / 2) : oam.tileID;
}

inline int getTileStride(const TengokuOAM& oam)
{
	return is8bppOAM(oam) ? (TILES_PER_LINE / 2) : TILES_PER_LINE;
}

inline int getTileIndexForOffset(const TengokuOAM& oam, int tileX, int tileY)
{
	return getTileBaseIndex(oam) + tileY * getTileStride(oam) + tileX;
}

inline int getTileIdFromBaseIndex(const TengokuOAM& oam, int baseIndex)
{
	return is8bppOAM(oam) ? (baseIndex * 2) : baseIndex;
}

static_assert(sizeof(TengokuOAM) == sizeof(uint16_t) * 3, "TengokuOAM must remain 3 packed attributes");

struct Palette {
	SDL_Color colors[16];
};

inline bool getOAMColor(const std::vector<Palette>& palettes, const TengokuOAM& oam, uint8_t colorIndex, SDL_Color& outColor)
{
	if (colorIndex == 0 || palettes.empty()) {
		return false;
	}

	if (is8bppOAM(oam)) {
		const int paletteIndex = colorIndex / 16;
		const int colorSlot = colorIndex % 16;
		if (paletteIndex >= static_cast<int>(palettes.size())) {
			return false;
		}
		outColor = palettes[paletteIndex].colors[colorSlot];
		return true;
	}

	if (oam.palette >= static_cast<int>(palettes.size()) || colorIndex >= 16) {
		return false;
	}

	outColor = palettes[oam.palette].colors[colorIndex];
	return true;
}

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
	void setTile(int index, const TileData& data);
	void ensureSize(int count);
	void resize(int count);

	TileData getTile(int index) const;

	int getSize() const;

	int getWidth(int tilesPerRow = TILES_PER_LINE) const;
	int getHeight(int tilesPerRow = TILES_PER_LINE) const;

	void clear();

private:
	std::vector<TileData> tiles;
};
