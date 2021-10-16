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

LabelMesh::LabelMesh()
{
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t), NULL, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(LabelVertex), NULL, GL_DYNAMIC_DRAW);

	// positions
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(LabelVertex), BUFFER_OFFSET(offsetof(LabelVertex, position)));
	glEnableVertexAttribArray(0);
	// texcoords
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(LabelVertex), BUFFER_OFFSET(offsetof(LabelVertex, texcoord)));
	glEnableVertexAttribArray(1);
}

LabelMesh::~LabelMesh()
{
	glDeleteBuffers(1, &EBO);
	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
}
	
void LabelMesh::set_text(const std::string &text, texture_font_t *font)
{
	buffer.vertices.clear();
	buffer.indices.clear();

	glm::vec2 pen = { 0.f, 0.f };

	glm::vec2 last = { 0.f, 0.f };

	LabelBuffer label;
		
	const uint32_t indices[6] = { 0, 1, 2, 0, 2, 3 };

	for (int i = 0; i < text.length(); i++) {
		texture_glyph_t *glyph = texture_font_get_glyph(font, &text.at(i));
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

		uint32_t offset = uint32_t(buffer.vertices.size()) + uint32_t(label.vertices.size());
		for (int j = 0; j < 6; j++) {
			label.indices.push_back(offset + indices[j]);
		}

		last = glm::vec2(x1, y1);

		LabelVertex vertices[4] = {
			{ { x0, y0 }, { s0, t0 } },
			{ { x0, y1 }, { s0, t1 } },
			{ { x1, y1 }, { s1, t1 } },
			{ { x1, y0 }, { s1, t0 } }
		};
		for (int j = 0; j < 4; j++) {
			vertices[j].position *= glm::vec2(0.1f, 0.1f);
			label.vertices.push_back(vertices[j]);
		}
		pen.x += float(glyph->advance_x);
	}

	bounds.min = glm::vec2((std::numeric_limits<float>::max)());
	bounds.max = glm::vec2((std::numeric_limits<float>::min)());

	float halfwidth = 0.5f * last.x;
	for (int i = 0; i < label.vertices.size(); i++) {
		// find bounds for background
		label.vertices[i].position.x -= 0.1f * halfwidth;
		bounds.min = (glm::min)(bounds.min, glm::vec2(label.vertices[i].position.x, label.vertices[i].position.y));
		bounds.max = (glm::max)(bounds.max, glm::vec2(label.vertices[i].position.x, label.vertices[i].position.y));
	}

	bounds.min -= glm::vec2(0.5f);
	bounds.max += glm::vec2(0.5f);

	buffer.indices.insert(buffer.indices.end(), label.indices.begin(), label.indices.end());
	buffer.vertices.insert(buffer.vertices.end(), label.vertices.begin(), label.vertices.end());

	// update buffer
	glBindVertexArray(VAO);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*buffer.indices.size(), buffer.indices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(LabelVertex)*buffer.vertices.size(), buffer.vertices.data(), GL_DYNAMIC_DRAW);
}
	
void LabelMesh::set_quad(const geom::Rectangle &rectangle)
{
	bounds = rectangle;

	buffer.vertices.clear();
	buffer.indices.clear();

	const glm::vec2 min = rectangle.min;
	const glm::vec2 max = rectangle.max;
	LabelVertex bottom_left = { { min.x, min.y }, { 0.f, 0.f } };
	LabelVertex bottom_right = { { max.x, min.y }, { 1.f, 0.f } };
	LabelVertex top_left = { { min.x, max.y }, { 0.f, 1.f } };
	LabelVertex top_right = { { max.x, max.y }, { 1.f, 1.f } };

	const uint32_t indices[6] = { 0, 1, 2, 0, 2, 3 };
	for (int j = 0; j < 6; j++) {
		buffer.indices.push_back(indices[j]);
	}

	buffer.vertices.push_back(bottom_right);
	buffer.vertices.push_back(top_right);
	buffer.vertices.push_back(top_left);
	buffer.vertices.push_back(bottom_left);

	// update buffer
	glBindVertexArray(VAO);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*buffer.indices.size(), buffer.indices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(LabelVertex)*buffer.vertices.size(), buffer.vertices.data(), GL_DYNAMIC_DRAW);
}

void LabelMesh::display() const
{
	glBindVertexArray(VAO);

	glDrawElements(GL_TRIANGLES, buffer.indices.size(), GL_UNSIGNED_INT, NULL);
}
	
LabelEntity::LabelEntity()
{
	text_mesh = std::make_unique<LabelMesh>();
	background_mesh = std::make_unique<LabelMesh>();
}
	
Labeler::Labeler(const std::string &fontpath, size_t fontsize, std::shared_ptr<Shader> shader)
	: m_shader(shader)
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
}

Labeler::~Labeler()
{
	glDeleteTextures(1, &m_atlas->id);

	texture_font_delete(m_font);

	texture_atlas_delete(m_atlas);
}

void Labeler::add_label(const geom::Transform *transform, float scale, const glm::vec3 &offset, const std::string &text, const glm::vec3 &color)
{
	auto entity = std::make_unique<LabelEntity>();
	entity->transform = transform;
	entity->scale = scale;
	entity->offset = offset;
	entity->text = text;
	entity->color = color;

	entity->text_mesh->set_text(text, m_font);
	entity->background_mesh->set_quad(entity->text_mesh->bounds);

	m_entities.push_back(std::move(entity));
}

void Labeler::clear()
{
	m_entities.clear();
}

void Labeler::display(const util::Camera &camera) const
{
	glDisable(GL_DEPTH_TEST);

	m_shader->use();
	m_shader->uniform_mat4("PROJECT", camera.projection);
	m_shader->uniform_mat4("VIEW", camera.viewing);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_atlas->id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_atlas->width, m_atlas->height, 0, GL_RED, GL_UNSIGNED_BYTE, m_atlas->data);

	for (const auto &entity : m_entities) {
		m_shader->uniform_float("SCALE", entity->scale);
		m_shader->uniform_vec3("ORIGIN", entity->transform->position + entity->offset);
		m_shader->uniform_vec3("COLOR", glm::vec3(0.f));
		entity->background_mesh->display();
	}

	for (const auto &entity : m_entities) {
		m_shader->uniform_float("SCALE", entity->scale);
		m_shader->uniform_vec3("ORIGIN", entity->transform->position + entity->offset);
		m_shader->uniform_vec3("COLOR", entity->color);
		entity->text_mesh->display();
	}

	glEnable(GL_DEPTH_TEST);
}

}
