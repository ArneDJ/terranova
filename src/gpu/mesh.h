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

class BufferObject {
public:
	BufferObject();
	~BufferObject();
public:
	void set_target(GLenum target);
	void bind();
	void store_immutable(GLsizei size, const void *data, GLbitfield flags);
	void store_mutable(GLsizei size, const void *data, GLenum usage);
private:
	GLuint m_buffer = 0;
	GLenum m_target = GL_ARRAY_BUFFER;
	GLsizei m_size = 0;
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
public:
	void create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
	void draw() const;
private:
	BufferObject m_vbo;
	BufferObject m_ebo;
	VertexArrayObject m_vao;
	Primitive m_primitive;
	GLenum m_index_type = GL_UNSIGNED_BYTE; // (GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT) // TODO automate with templates
};

class CubeMesh : public Mesh {
public:
	CubeMesh(const glm::vec3 &min, const glm::vec3 &max);
};

};
