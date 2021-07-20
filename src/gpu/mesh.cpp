#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

#include "../geom/frustum.h"
#include "../geom/geom.h"
#include "mesh.h"

namespace gpu {

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static size_t typesize(GLenum type);

BufferObject::BufferObject()
{
	glGenBuffers(1, &m_buffer);
}

BufferObject::~BufferObject()
{
	glDeleteBuffers(1, &m_buffer);
}
	
void BufferObject::set_target(GLenum target)
{
	glBindBuffer(target, m_buffer);
	m_target = target;
}

void BufferObject::bind() const
{
	glBindBuffer(m_target, m_buffer);
}

void BufferObject::bind_base(GLuint index) const
{
	glBindBufferBase(m_target, index, m_buffer);
}

void BufferObject::bind_explicit(GLenum target, GLuint index) const
{
	glBindBuffer(target, m_buffer);
	glBindBufferBase(target, index, m_buffer);
}
	
void BufferObject::store_immutable(GLsizei size, const void *data, GLbitfield flags)
{
	glBindBuffer(m_target, m_buffer);
	glBufferStorage(m_target, size, data, flags);
	m_size = size;
}
	
void BufferObject::store_mutable(GLsizei size, const void *data, GLenum usage)
{
	glBindBuffer(m_target, m_buffer);
	glBufferData(m_target, size, data, usage);
	m_size = size;
}

VertexArrayObject::VertexArrayObject()
{
	glGenVertexArrays(1, &m_array);
}

VertexArrayObject::~VertexArrayObject()
{
	glDeleteVertexArrays(1, &m_array);
}

void VertexArrayObject::bind() const
{
	glBindVertexArray(m_array);
}

void VertexArrayObject::set_attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer)
{
	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void Mesh::create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
	const size_t vertices_size = sizeof(Vertex) * vertices.size();
	const size_t indices_size = sizeof(uint32_t) * indices.size();

	m_primitive.first_index = 0;
	m_primitive.index_count = GLsizei(indices.size());
	m_primitive.first_vertex = 0;
	m_primitive.vertex_count = GLsizei(vertices.size());
	m_primitive.mode = GL_TRIANGLES;
	m_primitive.indexed = (indices.size() > 0);

	m_vao.bind();

	// add index buffer
	if (m_primitive.indexed) {
		m_ebo.set_target(GL_ELEMENT_ARRAY_BUFFER);
		m_ebo.store_mutable(indices_size, indices.data(), GL_STATIC_DRAW);
		m_index_type = GL_UNSIGNED_INT;
	}

	// add position buffer
	m_vbo.set_target(GL_ARRAY_BUFFER);
	m_vbo.store_mutable(vertices_size, vertices.data(), GL_STATIC_DRAW);

	m_vao.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(offsetof(Vertex, position)));
	m_vao.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(offsetof(Vertex, normal)));

	// indirect draw commands
	m_dbo.set_target(GL_DRAW_INDIRECT_BUFFER);

	m_ssbo.set_target(GL_SHADER_STORAGE_BUFFER);

	// find base bounding radius
	glm::vec3 min = glm::vec3((std::numeric_limits<float>::max)());
	glm::vec3 max = glm::vec3((std::numeric_limits<float>::min)());
	for (const auto &vertex : vertices) {
		min = (glm::min)(vertex.position, min);
		max = (glm::max)(vertex.position, max);
	}

	m_base_radius = 0.5f * glm::distance(min, max);
}
	
void Mesh::add_transform(const glm::vec3 &position, const glm::vec3 &scale)
{
	// the radius of the cull sphere is max scale multiplied by base scale
	float radius = m_base_radius * geom::max(scale.x, scale.y, scale.z);

	m_cull_spheres.push_back(glm::vec4(position, radius));

	struct DrawElementsCommand command;
	command.count = m_primitive.index_count;
	command.instance_count = 1;
	command.first_index = 0;
	command.base_vertex = 0;
	command.base_instance = m_draw_commands.size();

	m_draw_commands.push_back(command);
}

void Mesh::cull_instances_naive(const geom::Frustum &frustum)
{
	for (int i = 0; i < m_cull_spheres.size(); i++) {
		auto &cmd = m_draw_commands[i];
		if (frustum.sphere_intersects(m_cull_spheres[i])) {
			cmd.instance_count = 1;
		} else {
			cmd.instance_count = 0;
		}
	}
}

void Mesh::update_commands()
{
	m_ssbo.store_mutable(sizeof(glm::vec4) * m_cull_spheres.size(), m_cull_spheres.data(), GL_DYNAMIC_DRAW);
	m_dbo.store_mutable(sizeof(DrawElementsCommand) * m_draw_commands.size(), m_draw_commands.data(), GL_DYNAMIC_DRAW);
}

void Mesh::draw() const
{
	m_vao.bind();

	if (m_primitive.indexed) {
		glDrawElementsBaseVertex(m_primitive.mode, m_primitive.index_count, m_index_type, (GLvoid *)((m_primitive.first_index)*typesize(m_index_type)), m_primitive.first_vertex);
	} else {
		glDrawArrays(m_primitive.mode, m_primitive.first_vertex, m_primitive.vertex_count);
	}
}

void Mesh::draw_indirect() const
{
	m_vao.bind();

	m_dbo.bind();
	m_ssbo.bind_base(0); // TODO automate binding
		
	if (m_primitive.indexed) {
		glMultiDrawElementsIndirect(m_primitive.mode, m_index_type, (GLvoid*)0, GLsizei(m_draw_commands.size()), 0);
	} else {
		glMultiDrawArraysIndirect(m_primitive.mode, (GLvoid*)0, GLsizei(m_draw_commands.size()), 0);
	}
}

CubeMesh::CubeMesh(const glm::vec3 &min, const glm::vec3 &max)
{
	std::vector<Vertex> vertices = {
		{ { min.x, min.y, max.z }, { 1.f, 0.f, 1.f } },
		{ { max.x, min.y, max.z }, { 1.f, 0.f, 1.f } },
		{ { max.x, max.y, max.z }, { 1.f, 0.f, 1.f } },
		{ { min.x, max.y, max.z }, { 1.f, 0.f, 1.f } },
		{ { min.x, min.y, min.z }, { 1.f, 0.f, 1.f } },
		{ { max.x, min.y, min.z }, { 1.f, 0.f, 1.f } },
		{ { max.x, max.y, min.z }, { 1.f, 0.f, 1.f } },
		{ { min.x, max.y, min.z }, { 1.f, 0.f, 1.f } }
	};
	// indices
	const std::vector<uint32_t> indices = {
		0, 1, 2, 2, 3, 0,
		1, 5, 6, 6, 2, 1,
		7, 6, 5, 5, 4, 7,
		4, 0, 3, 3, 7, 4,
		4, 5, 1, 1, 0, 4,
		3, 2, 6, 6, 7, 3
	};

	create(vertices, indices);
}
	
static size_t typesize(GLenum type)
{
	switch (type) {
	case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
	case GL_UNSIGNED_SHORT: return sizeof(GLushort);
	case GL_UNSIGNED_INT: return sizeof(GLuint);
	};

	return 0;
}

};
