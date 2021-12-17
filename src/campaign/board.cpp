#include <vector>
#include <random>
#include <memory>
#include <list>
#include <queue>
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
#include "../util/image.h"
#include "../graphics/mesh.h"
#include "../graphics/shader.h"
#include "../graphics/texture.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "atlas.h"
#include "board.h"

void BoardMesh::clear()
{
	m_vertices.clear();
	m_tile_targets.clear();
	m_triangle_targets.clear();
}

void BoardMesh::add_cell(const geom::VoronoiCell &cell, const glm::vec3 &color)
{
	auto &targets = m_tile_targets[cell.index];

	for (const auto &edge : cell.edges) {
		BoardMeshVertex vertex_a = {
			edge->left_vertex->position,
			color,
			glm::vec3(1.f)
		};
		BoardMeshVertex vertex_b = {
			edge->right_vertex->position,
			color,
			glm::vec3(1.f)
		};
		BoardMeshVertex vertex_c = {
			cell.center,
			color,
			glm::vec3(1.f)
		};
		if (!geom::clockwise(edge->left_vertex->position, edge->right_vertex->position, cell.center)) {
			std::swap(vertex_a.position, vertex_b.position);
		}

		// add references
		TriangleKey key = std::make_pair(cell.index, edge->index);

		auto left_index = m_vertices.size();
		targets.push_back(left_index);
		m_vertices.push_back(vertex_a);

		auto right_index = m_vertices.size();
		targets.push_back(right_index);
		m_vertices.push_back(vertex_b);

		m_triangle_targets[key] = std::make_pair(left_index, right_index);

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
	m_vao.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(BoardMeshVertex), (char*)(offsetof(BoardMeshVertex, tile_color)));
	m_vao.set_attribute(2, 3, GL_FLOAT, GL_FALSE, sizeof(BoardMeshVertex), (char*)(offsetof(BoardMeshVertex, edge_color)));
}
	
void BoardMesh::refresh()
{
	const size_t data_size = sizeof(BoardMeshVertex) * m_vertices.size();
	m_vbo.store_mutable_part(0, data_size, m_vertices.data());
}

void BoardMesh::draw() const
{
	m_vao.bind();

	glPatchParameteri(GL_PATCH_VERTICES, 3);
	glDrawArrays(GL_PATCHES, 0, m_vertices.size());
}

void BoardMesh::color_tile(uint32_t tile, const glm::vec3 &color)
{
	auto search = m_tile_targets.find(tile);
	if (search != m_tile_targets.end()) {
		for (const auto &index : search->second) {
			m_vertices[index].tile_color = color;
		}
	}
}
	
void BoardMesh::color_border(uint32_t tile, uint32_t border, const glm::vec3 &color)
{
	TriangleKey key = std::make_pair(tile, border);
	auto search = m_triangle_targets.find(key);
	if (search != m_triangle_targets.end()) {
		auto &edge = search->second;
		m_vertices[edge.first].edge_color = color;
		m_vertices[edge.second].edge_color = color;
	}
}

BoardModel::BoardModel(std::shared_ptr<gfx::Shader> shader, const util::Image<uint8_t> &heightmap)
	: m_shader(shader)
{
	m_texture.create(heightmap);
}
	
void BoardModel::set_scale(const glm::vec3 &scale)
{
	m_scale = scale;
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
	
	m_texture.reload(atlas.heightmap());
}
	
void BoardModel::color_tile(uint32_t tile, const glm::vec3 &color)
{
	m_mesh.color_tile(tile, color);
}
	
void BoardModel::color_border(uint32_t tile, uint32_t border, const glm::vec3 &color)
{
	m_mesh.color_border(tile, border, color);
}
	
void BoardModel::update_mesh()
{
	m_mesh.refresh();
}

void BoardModel::display(const util::Camera &camera) const
{
	m_shader->use();
	m_shader->uniform_mat4("CAMERA_VP", camera.VP);
	m_shader->uniform_vec3("CAMERA_POSITION", camera.position);
	m_shader->uniform_vec3("MAP_SCALE", m_scale);
	// marker
	m_shader->uniform_vec2("MARKER_POS", m_marker.position);
	m_shader->uniform_vec3("MARKER_COLOR", m_marker.color);
	m_shader->uniform_float("MARKER_RADIUS", m_marker.radius);
	m_shader->uniform_float("MARKER_FADE", m_marker.fade);
	
	m_texture.bind(GL_TEXTURE0);

	m_mesh.draw();
}
	
void BoardModel::set_marker(const BoardMarker &marker)
{
	m_marker = marker;
}
	
void BoardModel::hide_marker()
{
	m_marker.fade = 0.f;
}
	
Board::Board(std::shared_ptr<gfx::Shader> tilemap)
	: m_model(tilemap, m_atlas.heightmap())
{
	m_height_field = std::make_unique<fysx::HeightField>(m_atlas.heightmap(), SCALE);
}
	
void Board::generate(int seed)
{
	AtlasParameters parameters = {};
	m_atlas.generate(seed, BOUNDS, parameters);
	
	build_navigation();
}
	
void Board::reload()
{
	m_model.reload(m_atlas);
	m_model.set_scale(SCALE);
}
	
void Board::paint_tile(uint32_t tile, const glm::vec3 &color)
{
	TilePaintJob job = { tile, color };
	m_paint_jobs.push(job);
}

void Board::paint_border(uint32_t tile, uint32_t border, const glm::vec3 &color)
{
	BorderPaintJob job = { tile, border, color };
	m_border_paint_jobs.push(job);
}

void Board::display(const util::Camera &camera)
{
	m_model.display(camera);
}

void Board::display_wireframe(const util::Camera &camera)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	m_model.display(camera);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
	
fysx::HeightField* Board::height_field()
{
	return m_height_field.get();
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
	
void Board::update()
{
	bool update_model = false;
	// color tiles
	if (!m_paint_jobs.empty()) {
		while (!m_paint_jobs.empty()) {
			auto order = m_paint_jobs.front();
			m_model.color_tile(order.tile, order.color);
			m_paint_jobs.pop();
		}

		update_model = true;
	}
	// color edges
	if (!m_border_paint_jobs.empty()) {
		while (!m_border_paint_jobs.empty()) {
			auto order = m_border_paint_jobs.front();
			m_model.color_border(order.tile, order.border, order.color);
			m_border_paint_jobs.pop();
		}
		update_model = true;
	}

	// tile colors have been changed, update mesh
	if (update_model) {
		m_model.update_mesh();
	}
}
	
void Board::build_navigation()
{
	std::vector<float> vertices;
	std::vector<int> indices;
	int index = 0;

	const auto &graph = m_atlas.graph();
	const auto &tiles = m_atlas.tiles();
	const auto &borders = m_atlas.borders();

	// create triangle data
	// tiles with rivers have adjusted triangles
	for (const auto &tile : tiles) {
		if (walkable_tile(&tile)) {
			const auto &cell = graph.cells[tile.index];
			for (const auto &edge : cell.edges) {
				const auto &border = borders[edge->index];
				indices.push_back(index);
				indices.push_back(index + 1);
				indices.push_back(index + 2);
			
				vertices.push_back(cell.center.x);
				vertices.push_back(0.f);
				vertices.push_back(cell.center.y);

				glm::vec2 a = edge->left_vertex->position;
				glm::vec2 b = edge->right_vertex->position;

				// rivers are not part of navigation mesh
				if (border.flags & BORDER_FLAG_RIVER) {
					a = geom::midpoint(a, cell.center);
					b = geom::midpoint(b, cell.center);
				}

				// TODO desired river width

				if (!geom::clockwise(cell.center, a, b)) {
					std::swap(a, b);
				}

				vertices.push_back(a.x);
				vertices.push_back(0.f);
				vertices.push_back(a.y);

				vertices.push_back(b.x);
				vertices.push_back(0.f);
				vertices.push_back(b.y);

				index += 3;
			}
		}
	}
	
	m_land_navigation.build(vertices, indices);
}

void Board::set_marker(const BoardMarker &marker)
{
	m_model.set_marker(marker);
}

void Board::hide_marker()
{
	m_model.hide_marker();
}
	
