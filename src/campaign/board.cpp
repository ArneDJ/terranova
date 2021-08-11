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
#include "../graphics/mesh.h"
#include "../graphics/shader.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"
#include "../navigation/astar.h"

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
	
void Board::find_path(const glm::vec2 &start, const glm::vec2 &end, std::list<glm::vec2> &pathways)
{
	auto origin = m_atlas.tile_at(start);
	auto target = m_atlas.tile_at(end);

	if (origin && target) {
		pathways.push_front(start);
		// same tile
		// early exit
		if (origin->index != target->index) {
			find_path(origin->index, target->index, pathways);
		}

		pathways.push_back(end);
	}
}

void Board::find_path(uint32_t start, uint32_t end, std::list<glm::vec2> &pathways)
{
	AStarSearch<nav::MapSearchNode> astarsearch;
	astarsearch.SetStartAndGoalStates(&nodes[start], &nodes[end]);

	unsigned int SearchState;
	unsigned int SearchSteps = 0;
	do {
		SearchState = astarsearch.SearchStep();
		SearchSteps++;
	} while (SearchState == AStarSearch<nav::MapSearchNode>::SEARCH_STATE_SEARCHING);

	std::cout << "Search steps " << SearchSteps << endl;
	std::cout << "Search state " << SearchState << endl;

	if (SearchState == AStarSearch<nav::MapSearchNode>::SEARCH_STATE_SUCCEEDED ) {

		nav::MapSearchNode *node = astarsearch.GetSolutionStart();

		int steps = 0;

		//node->PrintNodeInfo();

		pathways.push_back(node->position);

		for (;;) {
			node = astarsearch.GetSolutionNext();
		
			if(!node) { break; }
			
			pathways.push_back(node->position);

			//node->PrintNodeInfo();
			steps ++;
		};

		//std::cout << "Solution steps " << steps << endl;

		// Once you're done with the solution you can free the nodes up
		astarsearch.FreeSolutionNodes();
	} else if (SearchState == AStarSearch<nav::MapSearchNode>::SEARCH_STATE_FAILED) {
		std::cout << "Search terminated. Did not find goal state\n";
	}
}
	
void Board::reload()
{
	m_model.reload(m_atlas, m_seed);

	// FIXME 
	nodes.clear();
	nodes.resize(m_atlas.graph().cells.size());
	for (const auto &cell : m_atlas.graph().cells) {
		nav::MapSearchNode node;
		node.position = cell.center;
		node.index = cell.index;
		nodes[cell.index] = node;
		for (const auto &neighbor : cell.neighbors) {
			nodes[cell.index].neighbors.push_back(&nodes[neighbor->index]);
		}
	}
}
	
void Board::display(const util::Camera &camera)
{
	m_model.display(camera);
}
	
fysx::HeightField& Board::height_field()
{
	return m_height_field;
}

const Tile* Board::tile_at(const glm::vec2 &position) const
{
	return m_atlas.tile_at(position);
}
