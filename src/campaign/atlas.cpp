#include <vector>
#include <random>
#include <memory>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/fastnoise/FastNoise.h"

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"

#include "atlas.h"
	
void Atlas::generate(int seed, const geom::Rectangle &bounds)
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
	fastnoise.SetFrequency(2.f * 0.001f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetGradientPerturbAmp(200.f);

	for (auto &tile : m_tiles) {
		glm::vec2 center = m_graph.cells[tile.index].center;
		float x = center.x; float y = center.y;
		fastnoise.GradientPerturbFractal(x, y);
		float height = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
		tile.height = 255 * height;
	}
}
	
void Atlas::clear()
{
	m_tiles.clear();
	m_corners.clear();
	m_borders.clear();

	m_graph.clear();
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