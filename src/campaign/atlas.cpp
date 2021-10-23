#include <vector>
#include <random>
#include <memory>
#include <queue>
#include <unordered_map>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/fastnoise/FastNoise.h"

#include "../extern/poisson/PoissonGenerator.h"

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"
#include "../util/image.h"

#include "atlas.h"
	
Atlas::Atlas()
{
	m_heightmap.resize(1024, 1024, util::COLORSPACE_GRAYSCALE);
}

void Atlas::generate(int seed, const geom::Rectangle &bounds, const AtlasParameters &parameters)
{
	// clear all data
	clear();

	// generate the graph
	std::mt19937 gen(seed);
	
	glm::vec2 scale = bounds.max - bounds.min;
	std::vector<glm::vec2> points;

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(8000, PRNG, false);
	for (const auto &position : positions) {
		glm::vec2 point = { scale.x * position.x, scale.y * position.y };
		point += bounds.min;
		points.push_back(point);
	}

	m_graph.generate(points, bounds, 2);

	// create tile map
	m_tiles.resize(m_graph.cells.size());
	for (const auto &cell : m_graph.cells) {
		Tile tile;
		tile.index = cell.index;
		m_tiles[cell.index] = tile;
	}
	// borders
	m_borders.resize(m_graph.edges.size());
	for (const auto &edge : m_graph.edges) {
		Border border;
		border.index = edge.index;
		m_borders[edge.index] = border;
	}
	// corners
	m_corners.resize(m_graph.vertices.size());
	for (const auto &vertex : m_graph.vertices) {
		Corner corner;
		corner.index = vertex.index;
		m_corners[vertex.index] = corner;
	}

	// create height map
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(parameters.noise_frequency);
	fastnoise.SetFractalOctaves(parameters.noise_octaves);
	fastnoise.SetFractalLacunarity(parameters.noise_lacunarity);
	fastnoise.SetPerturbFrequency(parameters.perturb_frequency);
	fastnoise.SetGradientPerturbAmp(parameters.perturb_amp);

	for (int i = 0; i < m_heightmap.width(); i++) {
		for (int j = 0; j < m_heightmap.height(); j++) {
			float x = i; float y = j;
			fastnoise.GradientPerturbFractal(x, y);
			float height = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			m_heightmap.plot(i, j, util::CHANNEL_RED, 255 * height);
		}
	}

	for (auto &tile : m_tiles) {
		glm::vec2 center = m_graph.cells[tile.index].center;
		float x = center.x / bounds.max.x; 
		float y = center.y / bounds.max.y;
		tile.height = m_heightmap.sample_relative(x, y, util::CHANNEL_RED);
		if (tile.height > parameters.mountains) {
			tile.relief = ReliefType::MOUNTAINS;
		} else if (tile.height > parameters.hills) {
			tile.relief = ReliefType::HILLS;
		} else if (tile.height > parameters.lowland) {
			tile.relief = ReliefType::LOWLAND;
		} else {
			tile.relief = ReliefType::SEABED;
		}
	}

	// relief floodfill
	floodfill_relief(64, ReliefType::SEABED, ReliefType::LOWLAND);
	floodfill_relief(4, ReliefType::MOUNTAINS, ReliefType::HILLS);

	remove_echoriads();

	mark_islands(64);
}
	
void Atlas::clear()
{
	m_tiles.clear();
	m_corners.clear();
	m_borders.clear();

	m_graph.clear();

	m_heightmap.wipe();
}
	
const geom::VoronoiGraph& Atlas::graph() const
{
	return m_graph;
}

const std::vector<Tile>& Atlas::tiles() const
{
	return m_tiles;
}

const std::vector<Corner>& Atlas::corners() const
{
	return m_corners;
}

const std::vector<Border>& Atlas::borders() const
{
	return m_borders;
}
	
const util::Image<uint8_t>& Atlas::heightmap() const
{
	return m_heightmap;
}

const Tile* Atlas::tile_at(const glm::vec2 &position) const
{
	auto search = m_graph.cell_at(position);

	if (search.found) {
		return &m_tiles[search.cell->index];
	} 

	return nullptr;
}
	
glm::vec2 Atlas::tile_center(uint32_t index) const
{
	return m_graph.cells[index].center;
}
		
void Atlas::floodfill_relief(unsigned max_size, ReliefType target, ReliefType replacement)
{
	const auto &cells = m_graph.cells;

	std::unordered_map<uint32_t, bool> visited;

	for (auto &root : m_tiles) {
		std::vector<Tile*> marked;
		if (visited[root.index] == false && root.relief == target) {
			// breadth first search
			visited[root.index] = true;
			std::queue<uint32_t> queue;
			queue.push(root.index);
			
			while (!queue.empty()) {
				uint32_t node = queue.front();
				queue.pop();

				marked.push_back(&m_tiles[node]);

				for (const auto &cell : cells[node].neighbors) {
					Tile *neighbor = &m_tiles[cell->index];
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						if (neighbor->relief == target) {
							queue.push(neighbor->index);
						}
					}
				}
			}
		}

		// now that the size is known replace with target
		if (marked.size() > 0 && marked.size() < max_size) {
			for (Tile *tile : marked) {
				tile->relief = replacement;
			}
		}
	}
}
	
void Atlas::remove_echoriads()
{
	const auto &cells = m_graph.cells;

	std::unordered_map<uint32_t, bool> visited;
	
	for (auto &root : m_tiles) {
		std::vector<Tile*> marked;
		if (visited[root.index] == false && walkable_tile(&root) == true) {
			bool found_water = false;
			visited[root.index] = true;
			std::queue<Tile*> queue;
			queue.push(&root);
			
			while (!queue.empty()) {
				Tile *tile = queue.front();
				queue.pop();

				marked.push_back(tile);
				
				for (const auto &cell : cells[tile->index].neighbors) {
					Tile *neighbor = &m_tiles[cell->index];
					if (neighbor->relief == ReliefType::SEABED) {
						found_water = true;
						break;
					}
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						if (walkable_tile(neighbor)) {
							queue.push(neighbor);
						}
					}
				}
			}

			if (found_water == false) {
				for (Tile *tile : marked) {
					tile->relief = ReliefType::MOUNTAINS;
				}
			}
		}
	}
}
	
void Atlas::mark_islands(unsigned max_size)
{
	const auto &cells = m_graph.cells;

	std::unordered_map<uint32_t, bool> visited;
	
	for (auto &root : m_tiles) {
		std::vector<Tile*> marked;
		if (visited[root.index] == false && walkable_tile(&root) == true) {
			bool found_water = false;
			visited[root.index] = true;
			std::queue<Tile*> queue;
			queue.push(&root);
			
			while (!queue.empty()) {
				Tile *tile = queue.front();
				queue.pop();

				marked.push_back(tile);
				
				for (const auto &cell : cells[tile->index].neighbors) {
					Tile *neighbor = &m_tiles[cell->index];
					if (neighbor->relief == ReliefType::SEABED) {
						found_water = true;
					}
					if (visited[neighbor->index] == false) {
						visited[neighbor->index] = true;
						if (walkable_tile(neighbor)) {
							queue.push(neighbor);
						}
					}
				}
			}

			if (found_water == true && marked.size() < max_size) {
				for (Tile *tile : marked) {
					tile->flags |= TILE_FLAG_ISLAND;
				}
			}
		}
	}
}

bool walkable_tile(const Tile *tile)
{
	return (tile->relief == ReliefType::LOWLAND || tile->relief == ReliefType::HILLS);
}
