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
#include "../graphics/mesh.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "world.h"

void WorldModel::reload(const geom::VoronoiGraph &graph, int seed)
{
	m_vertices.clear();
	m_indices.clear();

	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis_color(0.2f, 1.2f);
	for (const auto &tile : graph.cells) {
		glm::vec3 color = { dis_color(gen), dis_color(gen), dis_color(gen) };
		for (const auto &edge : tile.edges) {
			gfx::Vertex vertex_a = {
				{ edge->left_vertex->position.x, 0.f, edge->left_vertex->position.y },
				color
			};
			gfx::Vertex vertex_b = {
				{ edge->right_vertex->position.x, 0.f, edge->right_vertex->position.y },
				color
			};
			gfx::Vertex vertex_c = {
				{ tile.center.x, 0.f, tile.center.y },
				color
			};
			if (geom::clockwise(edge->left_vertex->position, edge->right_vertex->position, tile.center)) {
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

void WorldModel::display() const
{
	m_mesh.draw();
}
	
void WorldMap::generate(int seed)
{
	m_seed = seed;

	std::mt19937 gen(seed);
	std::uniform_real_distribution<float> dis_x(BOUNDS.min.x, BOUNDS.max.x);
	std::uniform_real_distribution<float> dis_y(BOUNDS.min.y, BOUNDS.max.y);
	
	std::vector<glm::vec2> points;
	for (int i = 0; i < 400; i++) {
		glm::vec2 point = { dis_x(gen), dis_y(gen) };
		points.push_back(point);
	}

	m_graph.generate(points, BOUNDS, 2);
}
	
void WorldMap::reload()
{
	m_model.reload(m_graph, m_seed);
}
	
void WorldMap::display()
{
	m_model.display();
}
	
fysx::HeightField& WorldMap::height_field()
{
	return m_height_field;
}
