namespace gfx {

struct LabelVertex {
	glm::vec3 position = {};
	glm::vec3 origin = {};
	glm::vec3 color = {};
	glm::vec2 texcoord = {};
};

struct LabelBuffer {
	std::vector<LabelVertex> vertices;
	std::vector<uint32_t> indices;
};

struct LabelMesh {
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint EBO = 0;
};

class Labeler {
public:
	Labeler(const std::string &fontpath, size_t fontsize);
	~Labeler();
public:
	void add_label(const std::string &text, const glm::vec3 &color, const glm::vec3 &position);
	void display(const util::Camera &camera) const;
	void clear();
private:
	texture_atlas_t *m_atlas = nullptr;
	texture_font_t *m_font = nullptr;
	LabelBuffer m_buffer;
	LabelMesh m_mesh;
	Shader m_shader;
};

};
