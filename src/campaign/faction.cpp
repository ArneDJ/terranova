#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <random>
#include <list>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"
#include "../util/image.h"

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

void Faction::add_town(uint32_t town)
{
	towns.push_back(town);

	towns.unique();
}

void Faction::remove_town(uint32_t town)
{
	towns.remove(town);
}

void FactionController::clear()
{
	tile_owners.clear();
	factions.clear();

	town_targets.clear();
	m_desirable_tiles.clear();
	
	while (!m_expansion_requests.empty()) m_expansion_requests.pop();
}

// uses breadth first search to select a radius of tiles around a selected tile to reserve them
void FactionController::target_town_tiles(const Tile &tile, const Atlas &atlas, int radius, std::unordered_map<uint32_t, bool> &visited, std::unordered_map<uint32_t, uint32_t> &depth)
{
	const auto &cells = atlas.graph().cells;	
	const auto &borders = atlas.borders();	
	const auto &tiles = atlas.tiles();	

	depth[tile.index] = 0;
	visited[tile.index] = true;

	std::queue<uint32_t> nodes;
	nodes.push(tile.index);

	while (!nodes.empty()) {
		auto node = nodes.front();
		nodes.pop();
		uint32_t layer = depth[node] + 1;
		if (layer < radius) {
			const auto &cell = cells[node];
			for (const auto &edge : cell.edges) {
				const auto &border = borders[edge->index];
				// if no river is between them
				if (!(border.flags & BORDER_FLAG_RIVER) && !(border.flags & BORDER_FLAG_FRONTIER)) {
					auto neighbor_index = edge->left_cell->index == node ? edge->right_cell->index : edge->left_cell->index;
					const Tile *neighbor = &tiles[neighbor_index];
					if (walkable_tile(neighbor) && !visited[neighbor->index]) {
						visited[neighbor->index] = true;
						depth[neighbor->index] = layer;
						nodes.push(neighbor->index);
					}
				}
			}
		}
	}
}

void FactionController::find_town_targets(const Atlas &atlas, int radius)
{
	const auto &cells = atlas.graph().cells;	
	const auto &borders = atlas.borders();	
	const auto &tiles = atlas.tiles();	

	// do breath first search
	//
	std::unordered_map<uint32_t, bool> visited;
	std::unordered_map<uint32_t, uint32_t> depth;

	// to shuffle 
	std::random_device rd;
	std::mt19937 gen(rd());

	// first do tiles near river and coast
	for (const auto &tile : tiles) {
		m_desirable_tiles[tile.index] = false;
		bool ideal_start = (tile.flags & TILE_FLAG_RIVER) && (tile.flags & TILE_FLAG_COAST);
		if (!visited[tile.index] && walkable_tile(&tile) && ideal_start) {
			// found target
			town_targets.push_back(tile.index);
			m_desirable_tiles[tile.index] = true;

			target_town_tiles(tile, atlas, radius, visited, depth);
		}
	}
	
	std::shuffle(town_targets.begin(), town_targets.end(), gen);
	
	// then tiles next to river but not coastal
	for (const auto &tile : tiles) {
		bool near_river = (tile.flags & TILE_FLAG_RIVER);
		if (!visited[tile.index] && walkable_tile(&tile) && near_river) {
			// found target
			town_targets.push_back(tile.index);
			m_desirable_tiles[tile.index] = true;

			target_town_tiles(tile, atlas, radius, visited, depth);
		}
	}
	
	std::shuffle(town_targets.begin(), town_targets.end(), gen);

	// lastly fill in the gaps
	for (const auto &tile : tiles) {
		if (!visited[tile.index] && walkable_tile(&tile)) {
			// found target
			town_targets.push_back(tile.index);
			m_desirable_tiles[tile.index] = true;
			
			target_town_tiles(tile, atlas, radius, visited, depth);
		}
	}

	std::shuffle(town_targets.begin(), town_targets.end(), gen);
}
	
// finds the closest desirable unoccupied town target for a faction to settle
// starting from a given tile
// returns the tile id of the target tile
// if it doesn't find a tile it will return 0
uint32_t FactionController::find_closest_town_target(const Atlas &atlas, Faction *faction, uint32_t origin_tile)
{
	const auto &cells = atlas.graph().cells;	
	const auto &tiles = atlas.tiles();	
	const auto &borders = atlas.borders();	

	std::unordered_map<uint32_t, bool> visited;
	visited[origin_tile] = true;

	std::queue<uint32_t> nodes;
	nodes.push(origin_tile);

	while (!nodes.empty()) {
		auto node = nodes.front();
		nodes.pop();
		//for (const auto &cell : cells[node].neighbors) {
		const auto &cell = cells[node];
		for (const auto &edge : cell.edges) {
			const auto &border = borders[edge->index];
			// if no river is between them
			if (!(border.flags & BORDER_FLAG_RIVER) && !(border.flags & BORDER_FLAG_FRONTIER)) {
				auto neighbor_index = edge->left_cell->index == node ? edge->right_cell->index : edge->left_cell->index;
				const Tile *neighbor = &tiles[neighbor_index];
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
	}

	return 0;
}
	
void FactionController::add_expand_request(uint32_t faction_id)
{
	m_expansion_requests.push(faction_id);
}
	
// finds the faction at the top of request queue and returns its id
// returns 0 if nothing found
uint32_t FactionController::top_request()
{
	if (m_expansion_requests.empty()) { return 0; }

	auto id = m_expansion_requests.front();
	m_expansion_requests.pop();

	// find out if it is valid faction
	auto search = factions.find(id);
	if (search != factions.end()) {
		// found a faction
		return search->second->id();
	}

	return 0;
}
