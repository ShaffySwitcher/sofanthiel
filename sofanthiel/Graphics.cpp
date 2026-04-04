#include "Graphics.h"

void Tiles::addTile(std::array<uint8_t, 32> data)
{
	TileData tileData;

	// Convert 32 bytes into 8x8 tile data
	for(int i = 0; i < 8; i++) {
		for(int j = 0; j < 8; j++) {
			tileData.data[i][j] = (data[i * 4 + j / 2] >> ((j % 2) * 4)) & 0x0F;
		}
	}

	this->tiles.push_back(tileData);
}

TileData Tiles::getTile(int index) const
{
	if(index < 0 || index >= this->getSize()) {
		SDL_Log("Tile index out of bounds: %d (size: %d)", index, this->getSize());
		TileData empty = {};
		memset(&empty, 0, sizeof(TileData));
		return empty;
	}
	return this->tiles[index];
}

int Tiles::getSize() const
{
	return this->tiles.size();
}

int Tiles::getWidth(int tilesPerRow) const
{
	if (tilesPerRow <= 0) {
		tilesPerRow = TILES_PER_LINE;
	}

	const int clampedTilesPerRow = tilesPerRow;
	const int visibleTiles = SDL_min(static_cast<int>(this->tiles.size()), clampedTilesPerRow);
	return visibleTiles * 8;
}

int Tiles::getHeight(int tilesPerRow) const
{
	if (tilesPerRow <= 0) {
		tilesPerRow = TILES_PER_LINE;
	}

	return ((static_cast<int>(this->tiles.size()) + tilesPerRow - 1) / tilesPerRow) * 8;
}

void Tiles::setTile(int index, const TileData& data)
{
	if (index < 0 || index >= static_cast<int>(this->tiles.size())) {
		SDL_Log("Tile index out of bounds for setTile: %d (size: %d)", index, static_cast<int>(this->tiles.size()));
		return;
	}
	this->tiles[index] = data;
}

void Tiles::ensureSize(int count)
{
	while (static_cast<int>(this->tiles.size()) < count) {
		TileData empty = {};
		memset(&empty, 0, sizeof(TileData));
		this->tiles.push_back(empty);
	}
}

void Tiles::resize(int count)
{
	if (count < 0) {
		count = 0;
	}
	this->tiles.resize(static_cast<size_t>(count));
}

void Tiles::clear()
{
	this->tiles.clear();
}
