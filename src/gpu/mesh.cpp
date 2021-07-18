#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

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

void BufferObject::bind()
{
	glBindBuffer(m_target, m_buffer);
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

void CubeMesh::create(const glm::vec3 &min, const glm::vec3 &max)
{
	std::vector<glm::vec3> positions = {
		{ min.x, min.y, max.z },
		{ max.x, min.y, max.z },
		{ max.x, max.y, max.z },
		{ min.x, max.y, max.z },
		{ min.x, min.y, min.z },
		{ max.x, min.y, min.z },
		{ max.x, max.y, min.z },
		{ min.x, max.y, min.z }
	};
	// indices
	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0,
		1, 5, 6, 6, 2, 1,
		7, 6, 5, 5, 4, 7,
		4, 0, 3, 3, 7, 4,
		4, 5, 1, 1, 0, 4,
		3, 2, 6, 6, 7, 3
	};

	const size_t position_size = sizeof(glm::vec3) * positions.size();
	const size_t indices_size = sizeof(uint16_t) * indices.size();

	m_primitive.first_index = 0;
	m_primitive.index_count = GLsizei(indices.size());
	m_primitive.first_vertex = 0;
	m_primitive.vertex_count = GLsizei(positions.size());
	m_primitive.mode = GL_TRIANGLES;
	m_primitive.indexed = (indices.size() > 0);

	m_vao.bind();

	// add index buffer
	if (m_primitive.indexed) {
		m_ebo.set_target(GL_ELEMENT_ARRAY_BUFFER);
		m_ebo.store_mutable(indices_size, indices.data(), GL_STATIC_DRAW);
		m_index_type = GL_UNSIGNED_SHORT;
	}

	// add position buffer
	m_vbo.set_target(GL_ARRAY_BUFFER);
	m_vbo.store_mutable(position_size, positions.data(), GL_STATIC_DRAW);

	m_vao.set_attribute(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
}
	
void CubeMesh::draw() const
{
	m_vao.bind();

	if (m_primitive.indexed) {
		glDrawElementsBaseVertex(m_primitive.mode, m_primitive.index_count, m_index_type, (GLvoid *)((m_primitive.first_index)*typesize(m_index_type)), m_primitive.first_vertex);
	} else {
		glDrawArrays(m_primitive.mode, m_primitive.first_vertex, m_primitive.vertex_count);
	}
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
