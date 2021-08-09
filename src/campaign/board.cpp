#include <vector>
#include <random>
#include <memory>
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
		if (height < 114) {
			color = glm::vec3(0.1f);
		} else if (height > 168) {
			color = glm::vec3(1.f);
		} else {
			color = glm::vec3(height / 255.f);
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

	m_atlas.generate(seed, BOUNDS);
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
