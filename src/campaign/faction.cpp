#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <list>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"

#include "atlas.h"
#include "faction.h"

uint32_t Faction::id() const { return m_id; };

glm::vec3 Faction::color() const { return m_color; };

int Faction::gold() const { return m_gold; };

void Faction::set_color(const glm::vec3 &color)
{
	m_color = color;
}

void Faction::set_id(uint32_t id) 
{ 
	m_id = id; 
};
	
void Faction::add_gold(int amount)
{
	m_gold += amount;
}

void FactionController::clear()
{
	tile_owners.clear();
	factions.clear();

	town_targets.clear();
	m_desirable_tiles.clear();
	
	while (!m_expansion_requests.empty()) m_expansion_requests.pop();
}

void FactionController::find_town_targets(const Atlas &atlas, int radius)
{
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	

	// do breath first search
	//
	std::unordered_map<uint32_t, bool> visited;
	std::unordered_map<uint32_t, uint32_t> depth;
	for (const auto &tile : tiles) {
		m_desirable_tiles[tile.index] = false;
		if (visited[tile.index] == false && walkable_tile(&tile)) {
			depth[tile.index] = 0;
			visited[tile.index] = true;
			// found target
			town_targets.push_back(tile.index);
			m_desirable_tiles[tile.index] = true;

			std::queue<uint32_t> nodes;
			nodes.push(tile.index);

			while (!nodes.empty()) {
				auto node = nodes.front();
				nodes.pop();
				uint32_t layer = depth[node] + 1;
				if (layer < radius) {
					for (const auto &cell : cells[node].neighbors) {
						const Tile *neighbor = &tiles[cell->index];
						if (walkable_tile(neighbor) && visited[neighbor->index] == false) {
							visited[neighbor->index] = true;
							depth[neighbor->index] = layer;
							nodes.push(neighbor->index);
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < 5; i++) {
		std::random_shuffle(town_targets.begin(), town_targets.end());
	}
}
	
// finds the closest desirable unoccupied town target for a faction to settle
// starting from a given tile
// returns the tile id of the target tile
// if it doesn't find a tile it will return 0
uint32_t FactionController::find_closest_town_target(const Atlas &atlas, Faction *faction, uint32_t origin_tile)
{
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	

	std::unordered_map<uint32_t, bool> visited;
	visited[origin_tile] = true;

	std::queue<uint32_t> nodes;
	nodes.push(origin_tile);

	while (!nodes.empty()) {
		auto node = nodes.front();
		nodes.pop();
		for (const auto &cell : cells[node].neighbors) {
			const Tile *neighbor = &tiles[cell->index];
			// is it a land tile and has it not been visited yet?
			if (walkable_tile(neighbor) && !visited[neighbor->index]) {
				visited[neighbor->index] = true;
				auto occupier = tile_owners[neighbor->index];
				// is the tile unoccupied?
				if (occupier == 0) {
					// if it is a desirable tile then we found what we were looking for
					if (m_desirable_tiles[neighbor->index]) {
						return neighbor->index;
					} else {
						nodes.push(neighbor->index);
					}
				} else if (occupier == faction->id()) {
					// occupied by this faction then simply add to queue
					nodes.push(neighbor->index);
				}
			}
		}
	}

	return 0;
}
