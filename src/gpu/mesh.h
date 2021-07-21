namespace gpu {

struct Vertex {
	glm::vec3 position = {};
	glm::vec3 normal = {};
};

struct Primitive {
	GLuint first_index = 0;
	GLsizei index_count = 0;
	GLuint first_vertex = 0;
	GLsizei vertex_count = 0;
	GLenum mode = GL_TRIANGLES; // rendering mode (GL_TRIANGLES, GL_PATCHES, etc)
	bool indexed = false;
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

class Mesh {
public:
	void create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
	void draw() const;
protected:
	BufferObject m_vbo;
	BufferObject m_ebo;
	VertexArrayObject m_vao;
	Primitive m_primitive;
	GLenum m_index_type = GL_UNSIGNED_INT;
};

class IndirectMesh : public Mesh {
public:
	IndirectMesh();
public:
	void find_bounding_sphere(const std::vector<Vertex> &vertices);
	void attach_transform(const geom::Transform *transform);
	void update_buffers();
	void draw() const;
public:
	uint32_t instance_count() const;
	void bind_for_dispatch(GLuint draw_index, GLuint transforms_index, GLuint matrices_index) const;
private:
	uint32_t m_instance_count = 0;
	float m_base_radius = 1.f;
	BufferDataPair<PaddedTransform> m_transforms;
	BufferDataPair<glm::mat4> m_model_matrices;
	BufferDataPair<DrawElementsCommand> m_draw_commands;
};

class CubeMesh : public IndirectMesh {
public:
	CubeMesh(const glm::vec3 &min, const glm::vec3 &max);
};

};
