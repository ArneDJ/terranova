#pragma once 
#include "../geometry/geometry.h"

namespace gfx {

struct Vertex {
	glm::vec3 position = {};
	glm::vec3 normal = {};
};

// raw unparsed buffer data to be sent directly to GPU
struct MeshBufferData {
	std::vector<uint8_t> indices;
	std::vector<uint8_t> positions;
	std::vector<uint8_t> normals;
	std::vector<uint8_t> texcoords;
	std::vector<uint8_t> joints;
	std::vector<uint8_t> weights;
};

struct Primitive {
	GLuint first_index = 0;
	GLsizei index_count = 0;
	GLuint first_vertex = 0;
	GLsizei vertex_count = 0;
	GLenum mode = GL_TRIANGLES; // rendering mode (GL_TRIANGLES, GL_PATCHES, etc)
	GLenum index_type = GL_UNSIGNED_INT;
	geom::AABB bounds;
};

struct DrawArraysCommand {
	GLuint count = 0;
	GLuint instance_count = 0;
	GLuint first_vertex = 0;
	GLuint base_instance = 0;
};

struct DrawElementsCommand {
	GLuint count = 0;
	GLuint instance_count = 0;
	GLuint first_index = 0;
	GLuint base_vertex = 0;
	GLuint base_instance = 0;
};

// these have to be padded to vec4 to be used in an SSBO properly
struct PaddedTransform {
	glm::vec4 position;
	glm::vec4 rotation;
	glm::vec4 scale;
};

class BufferObject {
public:
	BufferObject();
	~BufferObject();
public:
	void set_target(GLenum target);
	void store_immutable(GLsizei size, const void *data, GLbitfield flags);
	void store_mutable(GLsizei size, const void *data, GLenum usage);
	void store_mutable_part(GLintptr offset, GLsizei size, const void *data);
public:
	void bind() const;
	void bind_base(GLuint index) const;
	void bind_explicit(GLenum target, GLuint index) const;
	GLsizei size() const;
private:
	GLenum m_target = GL_ARRAY_BUFFER;
	GLsizei m_size = 0;
	GLuint m_buffer = 0;
};

class VertexArrayObject {
public:
	VertexArrayObject();
	~VertexArrayObject();
public:
	void bind() const;
	void set_attribute(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer);
	void set_integer_attribute(GLuint index, GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
private:
	GLuint m_array = 0;
};

template <class T> class BufferDataPair {
public:
	BufferObject buffer;
	std::vector<T> data;
public:
	// only resize data on GPU if necessary
	bool resize_necessary()
	{
		const auto data_size = sizeof(T) * data.size();
		if (buffer.size() != data_size) {
			// size has changed, resize on GPU
			buffer.store_mutable(data_size, data.data(), GL_DYNAMIC_DRAW);
			return true;
		}

		// size hasn't changed
		return false;
	}
	// explicit data update to GPU
	void update_present()
	{
		if (!resize_necessary()) {
			const auto data_size = sizeof(T) * data.size();
			buffer.store_mutable_part(0, data_size, data.data());
		}
	} 
};

// manages indirect draw commands of glDrawArrays
class IndirectDrawer {
public:
	IndirectDrawer(const Primitive &primitive);
	void add_command();
	void pop_command();
	void update_buffer();
	void bind_for_culling(GLuint commands_index, GLuint sphere_index) const;
	void draw() const;
private:
	uint32_t m_instance_count = 0;
	Primitive m_primitive;
	geom::Sphere m_bounding_sphere;
	BufferObject m_sphere_ubo;
	BufferDataPair<DrawArraysCommand> m_commands;
};

// manages indirect draw commands of glDrawElements
class IndirectElementsDrawer {
public:
	IndirectElementsDrawer(const Primitive &primitive);
	void add_command();
	void pop_command();
	void update_buffer();
	void bind_for_culling(GLuint commands_index, GLuint sphere_index) const;
	void draw() const;
private:
	uint32_t m_instance_count = 0;
	Primitive m_primitive;
	geom::Sphere m_bounding_sphere;
	BufferObject m_sphere_ubo;
	BufferDataPair<DrawElementsCommand> m_commands;
};

class Mesh {
public:
	void create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, GLenum mode = GL_TRIANGLES);
	void create(const MeshBufferData &data, const std::vector<Primitive> &primitives);
	void draw() const;
	void bind_vao() const;
	const std::vector<Primitive>& primitives() const;
	const geom::AABB& bounds() const;
protected:
	geom::AABB m_bounding_box;
	BufferObject m_vbo;
	BufferObject m_ebo;
	VertexArrayObject m_vao;
	std::vector<Primitive> m_primitives;
	GLenum m_index_type = GL_UNSIGNED_INT;
};

GLenum index_type(size_t size);

};
