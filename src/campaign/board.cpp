#include <vector>
#include <chrono>
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

#define INT_CEIL(n,d) (int)ceil((float)n/d)
	
void BoardModel::paint_political_triangle(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c, const glm::vec3 &color, float alpha)
{
	std::vector<glm::ivec2> pixels;
	m_political_map.find_triangle_pixels_relative(a, b, c, pixels);
	for (const auto &pixel : pixels) {
		m_political_map.plot(pixel.x, pixel.y, util::CHANNEL_RED, 255 * color.x);
		m_political_map.plot(pixel.x, pixel.y, util::CHANNEL_GREEN, 255 * color.y);
		m_political_map.plot(pixel.x, pixel.y, util::CHANNEL_BLUE, 255 * color.z);
		m_political_map.plot(pixel.x, pixel.y, util::CHANNEL_ALPHA, 255 * alpha);
	}
}

void BoardModel::paint_political_line(const glm::vec2 &a, const glm::vec2 &b, uint8_t color)
{
	m_political_boundaries.draw_thick_line_relative(a, b, 2, util::CHANNEL_RED, color);
}

BoardModel::BoardModel(std::shared_ptr<gfx::Shader> shader, std::shared_ptr<gfx::Shader> blur_shader, const util::Image<float> &heightmap, const util::Image<float> &normalmap)
	: m_shader(shader), m_blur_shader(blur_shader)
{
	m_border_map.resize(2048, 2048, util::COLORSPACE_GRAYSCALE);
	m_political_map.resize(2048, 2048, util::COLORSPACE_RGBA);
	m_political_boundaries.resize(2048, 2048, util::COLORSPACE_GRAYSCALE);

	m_heightmap.create(heightmap);
	m_normalmap.create(normalmap);
	
	m_border_texture.create(m_border_map);
	m_political_texture.create(m_political_map);
	m_political_boundaries_raw.create(m_political_boundaries);
	m_political_boundaries_blurred.create(m_political_boundaries);
	
	// sample method to prevent black spots
	m_political_texture.change_filtering(GL_NEAREST);

	add_material("DISPLACEMENT", &m_heightmap);
	add_material("NORMALMAP", &m_normalmap);
	add_material("BORDERS", &m_border_texture);
	add_material("POLITICAL", &m_political_texture);
	add_material("BOUNDARIES", &m_political_boundaries_blurred);

	// create the mesh
	geom::Rectangle rectangle = { { 0.f, 0.f }, { 1024.f, 1024.f } };
	m_mesh.create(32, rectangle);
}
	
void BoardModel::set_scale(const glm::vec3 &scale)
{
	m_scale = scale;

	geom::Rectangle rectangle = { { 0.f, 0.f }, { scale.x, scale.z } };
	m_mesh.create(32, rectangle);
}

void BoardModel::add_material(const std::string &name, const gfx::Texture *texture)
{
	m_materials[name] = texture;
}

void BoardModel::bind_textures() const
{
	int location = 0;
	for (const auto &bucket : m_materials) {
		m_shader->uniform_int(bucket.first.c_str(), location);
		bucket.second->bind(GL_TEXTURE0 + location);
		location++;
    	}
}

void BoardModel::reload(const Atlas &atlas)
{
	m_border_map.wipe();
	m_political_map.wipe();
	m_political_boundaries.wipe();

	const auto &graph = atlas.graph();
	const auto &tiles = atlas.tiles();
	const auto &borders = atlas.borders();

	// create border map
	const auto &bounds = atlas.bounds();
	for (const auto &border : borders) {
		const auto &edge = graph.edges[border.index];
		auto &left_tile = tiles[edge.left_cell->index];
		auto &right_tile = tiles[edge.right_cell->index];
		if (walkable_tile(&left_tile) || walkable_tile(&right_tile)) {
			const auto &left_vertex = edge.left_vertex;
			const auto &right_vertex = edge.right_vertex;
			glm::vec2 a = left_vertex->position / bounds.max;
			glm::vec2 b = right_vertex->position / bounds.max;
			m_border_map.draw_line_relative(a, b, util::CHANNEL_RED, 255);
		}
	}
	m_border_map.blur(1.f);

	m_heightmap.reload(atlas.heightmap());
	m_normalmap.reload(atlas.normalmap());

	m_border_texture.reload(m_border_map);
}
	
void BoardModel::update()
{
	// send image data to GPU
	m_political_texture.reload(m_political_map);
	m_political_boundaries_blurred.reload(m_political_boundaries);
	// now blur the image data on GPU using a compute shader
	glm::vec2 resolution = { 
		m_political_boundaries.width(), 
		m_political_boundaries.height() 
	};

	m_blur_shader->use();
	
	GLuint ping = m_political_boundaries_blurred.binding();
	GLuint pong = m_political_boundaries_raw.binding();

	for (int i = 0; i < 6; i++) {
		glBindImageTexture(0, ping, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
		glBindImageTexture(1, pong, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

		m_blur_shader->uniform_vec2("DIR", glm::vec2(1.f, 0.f));

		glDispatchCompute(INT_CEIL(resolution.x, 16), INT_CEIL(resolution.y, 16), 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glBindImageTexture(0, pong, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
		glBindImageTexture(1, ping, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

		m_blur_shader->uniform_vec2("DIR", glm::vec2(0.f, 1.f));

		glDispatchCompute(INT_CEIL(resolution.x, 16), INT_CEIL(resolution.y, 16), 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}
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

	// borders
	m_shader->uniform_float("BORDER_MIX", m_border_mix);

	m_shader->uniform_float("POLITICAL_MIX", m_political_mix);
	
	bind_textures();

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
	
void BoardModel::set_border_mix(float mix)
{
	m_border_mix = mix;
}
	
Board::Board(std::shared_ptr<gfx::Shader> tilemap, std::shared_ptr<gfx::Shader> blur_shader)
	: m_model(tilemap, blur_shader, m_atlas.heightmap(), m_atlas.normalmap())
{
	m_height_field = std::make_unique<fysx::HeightField>(m_atlas.heightmap(), scale);
}
	
void Board::generate(int seed, const AtlasParameters& atlas_params)
{
	const geom::Rectangle bounds = { { 0.F, 0.F }, { scale.x, scale.z } };
	//AtlasParameters parameters = {};
	//parameters.tile_count = tile_count;
	m_atlas.generate(seed, bounds, atlas_params);
	
	build_navigation();
}
	
void Board::reload()
{
	m_height_field = std::make_unique<fysx::HeightField>(m_atlas.heightmap(), scale);

	// world normalmap from heightmap
	m_atlas.create_normalmap();
	
	m_model.reload(m_atlas);
	m_model.set_scale(scale);
}
	
void Board::paint_tile(uint32_t tile, const glm::vec3 &color, float alpha)
{
	TilePaintJob job = { tile, color, alpha };
	m_paint_jobs.push(job);
}

void Board::paint_border(uint32_t border, uint8_t color)
{
	BorderPaintJob job = { border, color };
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
	const auto &tiles = m_atlas.tiles();
	const auto &borders = m_atlas.borders();
	const auto &graph = m_atlas.graph();
	const auto &cells = graph.cells;
	const auto &edges = graph.edges;
	const auto &bounds = m_atlas.bounds();

	bool update_model = false;
	// color tiles
	if (!m_paint_jobs.empty()) {
		while (!m_paint_jobs.empty()) {
			auto order = m_paint_jobs.front();
			const auto &tile = tiles[order.tile];
			glm::vec2 center = cells[tile.index].center / bounds.max;
			const auto &cell = cells[tile.index];
			for (auto &edge : cell.edges) {
				const auto &left_vertex = edge->left_vertex;
				const auto &right_vertex = edge->right_vertex;
				glm::vec2 a = left_vertex->position / bounds.max;
				glm::vec2 b = right_vertex->position / bounds.max;
				m_model.paint_political_triangle(a, b, center, order.color, order.alpha);
			}
			m_paint_jobs.pop();
		}

		update_model = true;
	}
	// color edges
	if (!m_border_paint_jobs.empty()) {
		while (!m_border_paint_jobs.empty()) {
			auto order = m_border_paint_jobs.front();
			const auto &border = borders[order.border];
			const auto &edge = graph.edges[border.index];
			const auto &left_vertex = edge.left_vertex;
			const auto &right_vertex = edge.right_vertex;
			glm::vec2 a = left_vertex->position / bounds.max;
			glm::vec2 b = right_vertex->position / bounds.max;
			m_model.paint_political_line(a, b, order.color);
			m_border_paint_jobs.pop();
		}
		update_model = true;
	}

	// tile colors have been changed, update mesh
	if (update_model) {
		m_model.update();
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

void Board::set_border_mix(float mix)
{
	m_model.set_border_mix(mix);
}
	
