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

class BufferObject {
public:
	BufferObject();
	~BufferObject();
public:
	void set_target(GLenum target);
	void bind() const;
	void bind_base(GLuint index) const;
	void bind_explicit(GLenum target, GLuint index) const;
	void store_immutable(GLsizei size, const void *data, GLbitfield flags);
	void store_mutable(GLsizei size, const void *data, GLenum usage);
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

class Mesh {
public: // TODO make private
	std::vector<DrawElementsCommand> m_draw_commands;
	std::vector<glm::vec4> m_transforms; // needs to be vec4 to fit in SSBO
	BufferObject m_dbo;
	BufferObject m_ssbo;
public:
	void create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
	void add_transform(const glm::vec3 &position, const glm::vec3 &scale);
	void cull_instances_naive(const geom::Frustum &frustum);
	void update_commands();
	void draw() const;
	void draw_indirect() const;
private:
	// FIXME it's getting a bit too crowded here
	float m_base_radius = 1.f;
	BufferObject m_vbo;
	BufferObject m_ebo;
	VertexArrayObject m_vao;
	Primitive m_primitive;
	GLenum m_index_type = GL_UNSIGNED_INT;
};

class CubeMesh : public Mesh {
public:
	CubeMesh(const glm::vec3 &min, const glm::vec3 &max);
};

};
