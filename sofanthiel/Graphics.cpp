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
	TileData tile;
	if(index < 0 || index >= this->getSize()) {
		SDL_Log("Index out of bounds: %d", index);
	}
	return this->tiles[index];
}

int Tiles::getSize() const
{
	return this->tiles.size();
}

int Tiles::getWidth() const
{
	return (this->tiles.size() >= 32 ? 32 : this->tiles.size()) * 8;
}

int Tiles::getHeight() const
{
	return ((this->tiles.size() + 31) / 32) * 8;
}

void Tiles::clear()
{
	this->tiles.clear();
}