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

void BoardMesh::clear()
{
	m_vertices.clear();
	m_tile_vertices.clear();
}

void BoardMesh::add_cell(const geom::VoronoiCell &cell, const glm::vec3 &color)
{
	auto &targets = m_tile_vertices[cell.index];

	for (const auto &edge : cell.edges) {
		BoardMeshVertex vertex_a = {
			edge->left_vertex->position,
			color,
			glm::vec3(1.f, 0.f, 0.f)
		};
		BoardMeshVertex vertex_b = {
			edge->right_vertex->position,
			color,
			glm::vec3(0.f, 1.f, 0.f)
		};
		BoardMeshVertex vertex_c = {
			cell.center,
			color,
			glm::vec3(0.f, 0.f, 1.f)
		};
		if (!geom::clockwise(edge->left_vertex->position, edge->right_vertex->position, cell.center)) {
			std::swap(vertex_a.position, vertex_b.position);
		}
		targets.push_back(m_vertices.size());
		m_vertices.push_back(vertex_a);
		targets.push_back(m_vertices.size());
		m_vertices.push_back(vertex_b);
		targets.push_back(m_vertices.size());
		m_vertices.push_back(vertex_c);
	}
}

void BoardMesh::create()
{
	const size_t size = sizeof(BoardMeshVertex) * m_vertices.size();

	m_primitive.first_index = 0;
	m_primitive.index_count = 0;
	m_primitive.first_vertex = 0;
	m_primitive.vertex_count = GLsizei(m_vertices.size());
	m_primitive.mode = GL_TRIANGLES;
	m_primitive.index_type = GL_UNSIGNED_INT;

	m_vao.bind();

	// add position buffer
	m_vbo.set_target(GL_ARRAY_BUFFER);
	m_vbo.store_mutable(size, m_vertices.data(), GL_STATIC_DRAW);

	m_vao.set_attribute(0, 2, GL_FLOAT, GL_FALSE, sizeof(BoardMeshVertex), (char*)(offsetof(BoardMeshVertex, position)));
	m_vao.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(BoardMeshVertex), (char*)(offsetof(BoardMeshVertex, color)));
	m_vao.set_attribute(2, 3, GL_FLOAT, GL_FALSE, sizeof(BoardMeshVertex), (char*)(offsetof(BoardMeshVertex, barycentric)));
}
	
void BoardMesh::refresh()
{
	const size_t data_size = sizeof(BoardMeshVertex) * m_vertices.size();
	m_vbo.store_mutable_part(0, data_size, m_vertices.data());
}

void BoardMesh::draw() const
{
	m_vao.bind();

	glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());
}

void BoardMesh::color_tile(uint32_t tile, const glm::vec3 &color)
{
	auto search = m_tile_vertices.find(tile);
	if (search != m_tile_vertices.end()) {
		for (const auto &index : search->second) {
			m_vertices[index].color *= color;
		}
	}
}

BoardModel::BoardModel(const gfx::Shader *shader)
{
	m_shader = shader;
}

void BoardModel::reload(const Atlas &atlas)
{
	m_mesh.clear();

	const auto &graph = atlas.graph();
	const auto &tiles = atlas.tiles();

	uint32_t index = 0;
	for (const auto &tile : tiles) {
		const auto &cell = graph.cells[tile.index];
		uint8_t height = tile.height;
		glm::vec3 color = { 0.f, 0.f, 0.f };
		switch (tile.relief) {
		case ReliefType::SEABED: color = glm::vec3(0.1f); break;
		case ReliefType::LOWLAND: color = glm::vec3(0.5f); break;
		case ReliefType::HILLS: color = glm::vec3(0.75f); break;
		case ReliefType::MOUNTAINS: color = glm::vec3(1.f); break;
		}

		m_mesh.add_cell(cell, color);
	}

	m_mesh.create();
}
	
void BoardModel::color_tile(uint32_t tile, const glm::vec3 &color)
{
	m_mesh.color_tile(tile, color);
}
	
void BoardModel::update_mesh()
{
	m_mesh.refresh();
}

void BoardModel::display(const util::Camera &camera) const
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
	
	build_navigation();
}
	
void Board::reload()
{
	m_model.reload(m_atlas);
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
	
glm::vec2 Board::tile_center(uint32_t index) const
{
	return m_atlas.tile_center(index);
}
	
void Board::find_path(const glm::vec2 &start, const glm::vec2 &end, std::list<glm::vec2> &path) const
{
	m_land_navigation.find_2D_path(start, end, path);
}
	
void Board::color_tile(uint32_t tile, const glm::vec3 &color)
{
	m_model.color_tile(tile, color);
}
	
void Board::update_model()
{
	m_model.update_mesh();
}
	
void Board::occupy_tiles(uint32_t start, uint32_t occupier, uint32_t radius, std::vector<uint32_t> &occupied_tiles)
{
	m_atlas.occupy_tiles(start, occupier, radius, occupied_tiles);
}
	
void Board::build_navigation()
{
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
		const auto &tile = tiles[cell.index];
		if (tile.relief == ReliefType::LOWLAND || tile.relief == ReliefType::HILLS) {
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
