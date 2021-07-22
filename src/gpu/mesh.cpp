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
#include "../geom/transform.h"
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
	
GLsizei BufferObject::size() const
{
	return m_size;
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

// When replacing the entire data store, consider using glBufferSubData rather than completely recreating the data store with glBufferData. This avoids the cost of reallocating the data store.
void BufferObject::store_mutable_part(GLintptr offset, GLsizei size, const void *data)
{
	glBindBuffer(m_target, m_buffer);
	glBufferSubData(m_target, offset, size, data);
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

	m_vao.bind();

	// add index buffer
	if (indices.size() > 0) {
		m_ebo.set_target(GL_ELEMENT_ARRAY_BUFFER);
		m_ebo.store_immutable(indices_size, indices.data(), 0);
		m_index_type = GL_UNSIGNED_INT;
	}

	// add position buffer
	m_vbo.set_target(GL_ARRAY_BUFFER);
	m_vbo.store_immutable(vertices_size, vertices.data(), 0);

	m_vao.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(offsetof(Vertex, position)));
	m_vao.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(offsetof(Vertex, normal)));

	// find bounding box and sphere
	glm::vec3 min = glm::vec3((std::numeric_limits<float>::max)());
	glm::vec3 max = glm::vec3((std::numeric_limits<float>::min)());
	for (const auto &vertex : vertices) {
		min = (glm::min)(vertex.position, min);
		max = (glm::max)(vertex.position, max);
	}

	m_primitive.bounding_box.min = min;
	m_primitive.bounding_box.max = max;

	m_bounding_box = m_primitive.bounding_box;
}
	
void Mesh::draw() const
{
	m_vao.bind();

	if (m_primitive.index_count > 0) {
		glDrawElementsBaseVertex(m_primitive.mode, m_primitive.index_count, m_index_type, (GLvoid *)((m_primitive.first_index)*typesize(m_index_type)), m_primitive.first_vertex);
	} else {
		glDrawArrays(m_primitive.mode, m_primitive.first_vertex, m_primitive.vertex_count);
	}
}
	
IndirectMesh::IndirectMesh()
{
	m_draw_commands.buffer.set_target(GL_DRAW_INDIRECT_BUFFER);

	m_transforms.buffer.set_target(GL_SHADER_STORAGE_BUFFER);
	m_model_matrices.buffer.set_target(GL_SHADER_STORAGE_BUFFER);
	
	m_sphere_ubo.set_target(GL_UNIFORM_BUFFER);
}

void IndirectMesh::set_bounding_sphere()
{
	m_bounding_sphere = AABB_to_sphere(m_bounding_box);

	// send the base bounding sphere as an UBO to the GPU
	m_sphere_ubo.store_mutable(sizeof(geom::Sphere), &m_bounding_sphere, GL_STATIC_DRAW);
}

void IndirectMesh::update_buffers()
{
	m_transforms.update_present();

	m_model_matrices.resize_necessary();

	m_draw_commands.resize_necessary();
}

void IndirectMesh::attach_transform(const geom::Transform *transform)
{
	PaddedTransform padded = {
		glm::vec4(transform->position, 1.f),
		glm::vec4(transform->rotation.x, transform->rotation.y, transform->rotation.z, transform->rotation.w),
		glm::vec4(transform->scale, 1.f)
	};
	m_transforms.data.push_back(padded);

	struct DrawElementsCommand command;
	command.count = m_primitive.index_count;
	command.instance_count = 1;
	command.first_index = 0;
	command.base_vertex = 0;
	command.base_instance = m_instance_count;

	m_draw_commands.data.push_back(command);

	m_model_matrices.data.push_back(transform->to_matrix());

	m_instance_count++;
}

void IndirectMesh::draw() const
{
	m_vao.bind();

	m_draw_commands.buffer.bind();
	m_model_matrices.buffer.bind_base(2); // TODO use persistent-Mapping
		
	if (m_primitive.index_count > 0) {
		glMultiDrawElementsIndirect(m_primitive.mode, m_index_type, (GLvoid*)0, GLsizei(m_instance_count), 0);
	} else {
		glMultiDrawArraysIndirect(m_primitive.mode, (GLvoid*)0, GLsizei(instance_count()), 0);
	}
}

uint32_t IndirectMesh::instance_count() const
{
	return m_instance_count;
}

void IndirectMesh::bind_for_dispatch() const
{
	// need to bind draw buffer as ssbo
	m_draw_commands.buffer.bind_explicit(GL_SHADER_STORAGE_BUFFER, 0);
	m_transforms.buffer.bind_base(1);
	m_model_matrices.buffer.bind_base(2);
	m_sphere_ubo.bind_base(3);
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
	set_bounding_sphere();
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
