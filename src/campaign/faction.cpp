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
}

void FactionController::find_town_targets(const Atlas &atlas, int radius)
{
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	

	std::unordered_map<uint32_t, bool> visited;
	std::unordered_map<uint32_t, uint32_t> depth;
	for (const auto &tile : tiles) {
		if (visited[tile.index] == false && walkable_tile(&tile)) {
			depth[tile.index] = 0;
			visited[tile.index] = true;
			town_targets.push_back(tile.index);

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
