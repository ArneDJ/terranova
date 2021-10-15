#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/freetypegl/freetype-gl.h"

#include "../geometry/frustum.h"
#include "../geometry/transform.h"
#include "../geometry/geometry.h"
#include "../util/camera.h"

#include "shader.h"
#include "label.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

namespace gfx {

Labeler::Labeler(const std::string &fontpath, size_t fontsize)
{
	m_atlas = texture_atlas_new(1024, 1024, 1);
	m_font = texture_font_new_from_file(m_atlas, fontsize, fontpath.c_str());

	// create the font atlas texture
	glGenTextures(1, &m_atlas->id);
	glBindTexture(GL_TEXTURE_2D, m_atlas->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_atlas->width, m_atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, m_atlas->data);

	// create the batch mesh
	glGenVertexArrays(1, &m_mesh.VAO);
	glBindVertexArray(m_mesh.VAO);

	//GLuint EBO;
	glGenBuffers(1, &m_mesh.EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t), NULL, GL_DYNAMIC_DRAW);

	//GLuint VBO;
	glGenBuffers(1, &m_mesh.VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_mesh.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(LabelVertex), NULL, GL_DYNAMIC_DRAW);

	// positions
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LabelVertex), BUFFER_OFFSET(offsetof(LabelVertex, position)));
	glEnableVertexAttribArray(0);
	// origin
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LabelVertex), BUFFER_OFFSET(offsetof(LabelVertex, origin)));
	glEnableVertexAttribArray(1);
	// color
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(LabelVertex), BUFFER_OFFSET(offsetof(LabelVertex, color)));
	glEnableVertexAttribArray(2);
	// texcoords
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(LabelVertex), BUFFER_OFFSET(offsetof(LabelVertex, texcoord)));
	glEnableVertexAttribArray(3);

	m_shader.compile("shaders/label.vert", GL_VERTEX_SHADER);
	m_shader.compile("shaders/label.frag", GL_FRAGMENT_SHADER);
	m_shader.link();
}

Labeler::~Labeler()
{
	// delete batch mesh
	glDeleteBuffers(1, &m_mesh.EBO);
	glDeleteBuffers(1, &m_mesh.VBO);
	glDeleteVertexArrays(1, &m_mesh.VAO);

	glDeleteTextures(1, &m_atlas->id);

	texture_font_delete(m_font);

	texture_atlas_delete(m_atlas);
}

void Labeler::add_label(const std::string &text, const glm::vec3 &color, const glm::vec3 &origin)
{
	glm::vec2 pen = { 0.f, 0.f };

	glm::vec2 last = { color.x, color.z };

	LabelBuffer label;
		
	const uint32_t indices[6] = { 0, 1, 2, 0, 2, 3 };

	for (int i = 0; i < text.length(); i++) {
		texture_glyph_t *glyph = texture_font_get_glyph(m_font, &text.at(i));
		float kerning = 0.f;
		if (i > 0) {
			char back = text.at(i-1);
			kerning = texture_glyph_get_kerning(glyph, &text.at(i-1));
		}
		pen.x += kerning;
		int x0  = int(pen.x + glyph->offset_x);
		int y0  = int(pen.y + glyph->offset_y);
		int x1  = int(x0 + glyph->width);
		int y1  = int(y0 - glyph->height);
		float s0 = glyph->s0;
		float t0 = glyph->t0;
		float s1 = glyph->s1;
		float t1 = glyph->t1;

		uint32_t offset = uint32_t(m_buffer.vertices.size()) + uint32_t(label.vertices.size());
		for (int j = 0; j < 6; j++) {
			label.indices.push_back(offset + indices[j]);
		}

		last = glm::vec2(x1, y1);

		LabelVertex vertices[4] = {
			{ { x0, y0, 0 }, origin, color, { s0, t0 } },
			{ { x0, y1, 0 }, origin, color, { s0, t1 } },
			{ { x1, y1, 0 }, origin, color, { s1, t1 } },
			{ { x1, y0, 0 }, origin, color, { s1, t0 } }
		};
		for (int j = 0; j < 4; j++) {
			vertices[j].position *= glm::vec3(0.1f, 0.1f, 0.1f);
			label.vertices.push_back(vertices[j]);
		}
		pen.x += float(glyph->advance_x);
	}

	glm::vec2 min = glm::vec2((std::numeric_limits<float>::max)());
	glm::vec2 max = glm::vec2((std::numeric_limits<float>::min)());

	float halfwidth = 0.5f * last.x;
	for (int i = 0; i < label.vertices.size(); i++) {
		// find bounds for background
		label.vertices[i].position.x -= 0.1f * halfwidth;
		min = (glm::min)(min, glm::vec2(label.vertices[i].position.x, label.vertices[i].position.y));
		max = (glm::max)(max, glm::vec2(label.vertices[i].position.x, label.vertices[i].position.y));
	}

	min -= glm::vec2(0.5f);
	max += glm::vec2(0.5f);

	// add background
	glm::vec3 background_color = { 0.f, 0.f, 0.f };
	LabelVertex bottom_left = { { min.x, min.y, 0 }, origin, background_color, { 0.f, 0.f } };
	LabelVertex bottom_right = { { max.x, min.y, 0 }, origin, background_color, { 0.f, 0.f } };
	LabelVertex top_left = { { min.x, max.y, 0 }, origin, background_color, { 0.f, 0.f } };
	LabelVertex top_right = { { max.x, max.y, 0 }, origin, background_color, { 0.f, 0.f } };
	uint32_t offset = uint32_t(m_buffer.vertices.size()) + uint32_t(label.vertices.size());
	for (int j = 0; j < 6; j++) {
		label.indices.push_back(offset + indices[j]);
	}

	label.vertices.push_back(bottom_right);
	label.vertices.push_back(top_right);
	label.vertices.push_back(top_left);
	label.vertices.push_back(bottom_left);

	m_buffer.indices.insert(m_buffer.indices.end(), label.indices.begin(), label.indices.end());
	m_buffer.vertices.insert(m_buffer.vertices.end(), label.vertices.begin(), label.vertices.end());
}

void Labeler::clear()
{
	m_buffer.vertices.clear();
	m_buffer.indices.clear();
}

void Labeler::display(const util::Camera &camera) const
{
	m_shader.use();
	m_shader.uniform_float("SCALE", 1.f);
	m_shader.uniform_mat4("PROJECT", camera.projection);
	m_shader.uniform_mat4("VIEW", camera.viewing);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_atlas->id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_atlas->width, m_atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, m_atlas->data);

	glBindVertexArray(m_mesh.VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*m_buffer.indices.size(), m_buffer.indices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, m_mesh.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(LabelVertex)*m_buffer.vertices.size(), m_buffer.vertices.data(), GL_DYNAMIC_DRAW);

	glDrawElements(GL_TRIANGLES, m_buffer.indices.size(), GL_UNSIGNED_INT, NULL);
}

}
