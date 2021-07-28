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

#include "../extern/loguru/loguru.hpp"

#include "../geometry/frustum.h"
#include "mesh.h"

namespace gfx {

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

void VertexArrayObject::set_integer_attribute(GLuint index, GLint size, GLenum type, GLsizei stride, GLvoid *pointer)
{
	glEnableVertexAttribArray(index);
	glVertexAttribIPointer(index, size, type, stride, pointer);
}
	
IndirectDrawer::IndirectDrawer(const Primitive &primitive)
{
	m_primitive = primitive;

	m_commands.buffer.set_target(GL_DRAW_INDIRECT_BUFFER);

	m_bounding_sphere = AABB_to_sphere(primitive.bounds);

	// send the base bounding sphere as an UBO to the GPU
	m_sphere_ubo.set_target(GL_UNIFORM_BUFFER);
	m_sphere_ubo.store_mutable(sizeof(geom::Sphere), &m_bounding_sphere, GL_STATIC_DRAW);
}

void IndirectDrawer::add_command()
{
	struct DrawArraysCommand command;

	command.count = m_primitive.vertex_count;
	command.instance_count = 1;
	command.first_vertex = m_primitive.first_vertex;
	command.base_instance = m_commands.data.size();

	m_commands.data.push_back(command);

	m_instance_count = m_commands.data.size();
}

void IndirectDrawer::pop_command()
{
	m_commands.data.pop_back();

	m_instance_count = m_commands.data.size();
}

void IndirectDrawer::update_buffer()
{
	m_commands.resize_necessary();
}

// need to bind command buffer as SSBO to edit in compute shader for culling
void IndirectDrawer::bind_for_culling(GLuint commands_index, GLuint sphere_index) const
{
	m_commands.buffer.bind_explicit(GL_SHADER_STORAGE_BUFFER, commands_index);
	m_sphere_ubo.bind_base(sphere_index);
}

void IndirectDrawer::draw() const
{
	m_commands.buffer.bind();
		
	glMultiDrawArraysIndirect(m_primitive.mode, (GLvoid*)0, m_instance_count, 0);
}

IndirectElementsDrawer::IndirectElementsDrawer(const Primitive &primitive)
{
	m_primitive = primitive;

	m_commands.buffer.set_target(GL_DRAW_INDIRECT_BUFFER);

	m_bounding_sphere = AABB_to_sphere(primitive.bounds);

	// send the base bounding sphere as an UBO to the GPU
	m_sphere_ubo.set_target(GL_UNIFORM_BUFFER);
	m_sphere_ubo.store_mutable(sizeof(geom::Sphere), &m_bounding_sphere, GL_STATIC_DRAW);
}

void IndirectElementsDrawer::add_command()
{
	struct DrawElementsCommand command;

	command.count = m_primitive.index_count;
	command.instance_count = 1;
	command.first_index = m_primitive.first_index;
	command.base_vertex = m_primitive.first_vertex;
	command.base_instance = m_commands.data.size();

	m_commands.data.push_back(command);

	m_instance_count = m_commands.data.size();
}

void IndirectElementsDrawer::pop_command()
{
	m_commands.data.pop_back();

	m_instance_count = m_commands.data.size();
}

void IndirectElementsDrawer::update_buffer()
{
	m_commands.resize_necessary();
}

// need to bind command buffer as SSBO to edit in compute shader for culling
void IndirectElementsDrawer::bind_for_culling(GLuint commands_index, GLuint sphere_index) const
{
	m_commands.buffer.bind_explicit(GL_SHADER_STORAGE_BUFFER, commands_index);
	m_sphere_ubo.bind_base(sphere_index);
}

void IndirectElementsDrawer::draw() const
{
	m_commands.buffer.bind();
		
	glMultiDrawElementsIndirect(m_primitive.mode, m_primitive.index_type, (GLvoid*)0, m_instance_count, 0);
}

void Mesh::create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, GLenum mode)
{
	m_primitives.clear();

	const size_t vertices_size = sizeof(Vertex) * vertices.size();
	const size_t indices_size = sizeof(uint32_t) * indices.size();

	Primitive primitive;
	primitive.first_index = 0;
	primitive.index_count = GLsizei(indices.size());
	primitive.first_vertex = 0;
	primitive.vertex_count = GLsizei(vertices.size());
	primitive.mode = mode;
	primitive.index_type = GL_UNSIGNED_INT;

	m_vao.bind();

	// add index buffer
	if (indices.size() > 0) {
		m_ebo.set_target(GL_ELEMENT_ARRAY_BUFFER);
		m_ebo.store_mutable(indices_size, indices.data(), GL_STATIC_DRAW);
		m_index_type = GL_UNSIGNED_INT;
	}

	// add position buffer
	m_vbo.set_target(GL_ARRAY_BUFFER);
	m_vbo.store_mutable(vertices_size, vertices.data(), GL_STATIC_DRAW);

	m_vao.set_attribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(offsetof(Vertex, position)));
	m_vao.set_attribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), BUFFER_OFFSET(offsetof(Vertex, normal)));

	// find bounding box and sphere
	glm::vec3 min = glm::vec3((std::numeric_limits<float>::max)());
	glm::vec3 max = glm::vec3((std::numeric_limits<float>::min)());
	for (const auto &vertex : vertices) {
		min = (glm::min)(vertex.position, min);
		max = (glm::max)(vertex.position, max);
	}

	primitive.bounds.min = min;
	primitive.bounds.max = max;

	m_bounding_box = primitive.bounds;

	m_primitives.push_back(primitive);
}
	
void Mesh::create(const MeshBufferData &data, const std::vector<Primitive> &primitives)
{
	m_primitives = primitives;

	// model bounding box
	m_bounding_box.min = glm::vec3((std::numeric_limits<float>::max)());
	m_bounding_box.max = glm::vec3((std::numeric_limits<float>::min)());
	for (const auto &primitive : primitives) {
		m_bounding_box = geom::confined_bounds(primitive.bounds, m_bounding_box);
	}

	m_vao.bind();

	// add index buffer
	if (data.indices.size() > 0) {
		m_ebo.set_target(GL_ELEMENT_ARRAY_BUFFER);
		m_ebo.store_immutable(data.indices.size(), data.indices.data(), 0);
	}

	// add position buffer
	m_vbo.set_target(GL_ARRAY_BUFFER);

	size_t total_size = data.positions.size() + data.normals.size() + data.texcoords.size() + data.joints.size() + data.weights.size();
	size_t offset = 0;

	m_vbo.store_mutable(total_size, nullptr, GL_STATIC_DRAW);

	// positions
	m_vao.set_attribute(0, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(offset));
	m_vbo.store_mutable_part(offset, data.positions.size(), data.positions.data());
	offset += data.positions.size();

	// normals
	m_vao.set_attribute(1, 3, GL_FLOAT, GL_TRUE, 0, BUFFER_OFFSET(offset));
	m_vbo.store_mutable_part(offset, data.normals.size(), data.normals.data());
	offset += data.normals.size();

	// texcoords
	m_vao.set_attribute(2, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(offset));
	m_vbo.store_mutable_part(offset, data.texcoords.size(), data.texcoords.data());
	offset += data.texcoords.size();

	// joints
	m_vao.set_integer_attribute(3, 4, GL_UNSIGNED_BYTE, 0, BUFFER_OFFSET(offset));
	m_vbo.store_mutable_part(offset, data.joints.size(), data.joints.data());
	offset += data.joints.size();

	// weights
	m_vao.set_attribute(4, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(offset));
	m_vbo.store_mutable_part(offset, data.weights.size(), data.weights.data());
	offset += data.weights.size();
}
	
void Mesh::draw() const
{
	m_vao.bind();

	for (const auto &primitive : m_primitives) {
		if (primitive.index_count > 0) {
			glDrawElementsBaseVertex(primitive.mode, primitive.index_count, primitive.index_type, (GLvoid *)((primitive.first_index)*typesize(primitive.index_type)), primitive.first_vertex);
		} else {
			glDrawArrays(primitive.mode, primitive.first_vertex, primitive.vertex_count);
		}
	}
}
	
void Mesh::bind_vao() const
{
	m_vao.bind();
}
	
const std::vector<Primitive>& Mesh::primitives() const
{
	return m_primitives;
}
	
const geom::AABB& Mesh::bounds() const
{
	return m_bounding_box;
}
	
GLenum index_type(size_t size)
{
	switch (size) {
	case sizeof(GLubyte): return GL_UNSIGNED_BYTE;
	case sizeof(GLushort): return GL_UNSIGNED_SHORT;
	case sizeof(GLuint): return GL_UNSIGNED_INT;
	default: return GL_BYTE;
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
