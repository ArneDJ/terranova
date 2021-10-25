namespace gfx {

struct LabelVertex {
	glm::vec2 position = {};
	glm::vec2 texcoord = {};
};

struct LabelBuffer {
	std::vector<LabelVertex> vertices;
	std::vector<uint32_t> indices;
};

class LabelMesh {
public:
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint EBO = 0;
	LabelBuffer buffer;
	geom::Rectangle bounds;
public:
	LabelMesh();
	~LabelMesh();
public:
	void set_text(const std::string &text, texture_font_t *font);
	void set_quad(const geom::Rectangle &rectangle);
	void display() const;
};

class LabelEntity {
public:
	LabelEntity();
public:
	const geom::Transform *transform = nullptr;
	float scale = 1.f;
	glm::vec3 offset = {}; // offset from the transform position
	glm::vec3 color = {};
	std::string text = {};
	std::unique_ptr<LabelMesh> text_mesh;
	std::unique_ptr<LabelMesh> background_mesh;
};

class Labeler {
public:
	Labeler(const std::string &fontpath, size_t fontsize, std::shared_ptr<Shader> shader);
	~Labeler();
public:
	void add_label(const geom::Transform *transform, float scale, const glm::vec3 &offset, const std::string &text, const glm::vec3 &color);
	void change_text_color(const geom::Transform *transform, const glm::vec3 &color);
	//void remove_label(const Transform *transform);
	void display(const util::Camera &camera) const;
	void clear();
private:
	texture_atlas_t *m_atlas = nullptr;
	texture_font_t *m_font = nullptr;
	std::shared_ptr<Shader> m_shader;
	std::vector<std::unique_ptr<LabelEntity>> m_entities;
};

};
