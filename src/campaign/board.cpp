#include <vector>
#include <random>
#include <memory>
#include <list>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"
#include "../util/camera.h"
#include "../util/navigation.h"
#include "../graphics/mesh.h"
#include "../graphics/shader.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "atlas.h"
#include "board.h"

WorldModel::WorldModel(const gfx::Shader *shader)
{
	m_shader = shader;
}

void WorldModel::reload(const Atlas &atlas, int seed)
{
	m_vertices.clear();
	m_indices.clear();

	const auto &graph = atlas.graph();
	const auto &tiles = atlas.tiles();

	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis_color(0.2f, 1.2f);
	for (const auto &tile : tiles) {
		const auto &cell = graph.cells[tile.index];
		uint8_t height = tile.height;
		glm::vec3 color = { dis_color(gen), dis_color(gen), dis_color(gen) };
		switch (tile.relief) {
		case ReliefType::SEABED: color = glm::vec3(0.1f); break;
		case ReliefType::LOWLAND: color = glm::vec3(0.5f); break;
		case ReliefType::HILLS: color = glm::vec3(0.75f); break;
		case ReliefType::MOUNTAINS: color = glm::vec3(1.f); break;
		}
		for (const auto &edge : cell.edges) {
			gfx::Vertex vertex_a = {
				{ edge->left_vertex->position.x, 0.f, edge->left_vertex->position.y },
				color
			};
			gfx::Vertex vertex_b = {
				{ edge->right_vertex->position.x, 0.f, edge->right_vertex->position.y },
				color
			};
			gfx::Vertex vertex_c = {
				{ cell.center.x, 0.f, cell.center.y },
				color
			};
			if (geom::clockwise(edge->left_vertex->position, edge->right_vertex->position, cell.center)) {
				m_vertices.push_back(vertex_a);
				m_vertices.push_back(vertex_b);
				m_vertices.push_back(vertex_c);
			} else {
				m_vertices.push_back(vertex_b);
				m_vertices.push_back(vertex_a);
				m_vertices.push_back(vertex_c);
			}
		}
	}

	m_mesh.create(m_vertices, m_indices);
}

void WorldModel::display(const util::Camera &camera) const
{
	m_shader->use();
	m_shader->uniform_mat4("CAMERA_VP", camera.VP);

	m_mesh.draw();
}
	
Board::Board(const gfx::Shader *tilemap)
	: m_model(tilemap)
{
}
	
void Board::generate(int seed)
{
	m_seed = seed;

	AtlasParameters parameters = {};
	m_atlas.generate(seed, BOUNDS, parameters);
	
	// build navigation mesh
	std::vector<float> vertices;
	std::vector<int> indices;
	int index = 0;

	const auto &graph = m_atlas.graph();
	for (const auto &vertex : graph.vertices) {
		vertices.push_back(vertex.position.x);
		vertices.push_back(0.f);
		vertices.push_back(vertex.position.y);
		index++;
	}
	const auto &tiles = m_atlas.tiles();
	for (const auto &cell : graph.cells) {
		if (tiles[cell.index].height >= 114 && tiles[cell.index].height <= 168) {
			for (const auto &edge : cell.edges) {
				indices.push_back(index);
				if (geom::clockwise(cell.center, edge->left_vertex->position, edge->right_vertex->position)) {
					indices.push_back(edge->left_vertex->index);
					indices.push_back(edge->right_vertex->index);
				} else {
					indices.push_back(edge->right_vertex->index);
					indices.push_back(edge->left_vertex->index);
				}
			}
			vertices.push_back(cell.center.x);
			vertices.push_back(0.f);
			vertices.push_back(cell.center.y);
			index++;
		}
	}

	m_land_navigation.build(vertices, indices);
}
	
void Board::reload()
{
	m_model.reload(m_atlas, m_seed);
}
	
void Board::display(const util::Camera &camera)
{
	m_model.display(camera);
}
	
fysx::HeightField& Board::height_field()
{
	return m_height_field;
}
	
const util::Navigation& Board::navigation() const
{
	return m_land_navigation;
}

const Tile* Board::tile_at(const glm::vec2 &position) const
{
	return m_atlas.tile_at(position);
}
	
void Board::find_path(const glm::vec2 &start, const glm::vec2 &end, std::list<glm::vec2> &path) const
{
	m_land_navigation.find_2D_path(start, end, path);
}
