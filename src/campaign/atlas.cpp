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
	std::uniform_real_distribution<float> dis_x(bounds.min.x, bounds.max.x);
	std::uniform_real_distribution<float> dis_y(bounds.min.y, bounds.max.y);
	
	std::vector<glm::vec2> points;
	for (int i = 0; i < 10000; i++) {
		glm::vec2 point = { dis_x(gen), dis_y(gen) };
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
	
// FIXME review
void Atlas::occupy_tiles(uint32_t start, uint32_t occupier, uint32_t radius, std::vector<uint32_t> &occupied_tiles)
{
	// breadth first search until radius
	std::unordered_map<uint32_t, bool> visited;
	std::unordered_map<uint32_t, uint32_t> depth;
	std::queue<Tile*> nodes;
	
	visited[start] = true;
	depth[start] = 0;

	Tile *root = &m_tiles[start]; // TODO safe check
	if (root->occupier == 0) {
		nodes.push(root);
	}

	while (!nodes.empty()) {
		Tile *node = nodes.front();
		nodes.pop();
		node->occupier = occupier;
		occupied_tiles.push_back(node->index);
		uint32_t layer = depth[node->index] + 1;
		if (layer < radius) {
			auto &cell = m_graph.cells[node->index];
			for (const auto &neighbor : cell.neighbors) {
				Tile *neighbor_tile = &m_tiles[neighbor->index];
				auto search = visited.find(neighbor->index);
				if (search == visited.end()) {
					visited[neighbor->index] = true;
					depth[neighbor->index] = layer;
					bool valid_relief = neighbor_tile->relief == ReliefType::LOWLAND || neighbor_tile->relief == ReliefType::HILLS;
					bool can_occupy = neighbor_tile->occupier == 0;
					if (can_occupy && valid_relief) {
						nodes.push(neighbor_tile);
					}
				}
			}
		}
	}
}
