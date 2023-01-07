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
	//void set_text(const std::string &text, texture_font_t *font);
	void set_quad(const geom::Rectangle &rectangle);
	void display() const;
};

class Label {
public:
	std::unique_ptr<LabelMesh> text_mesh;
	std::unique_ptr<LabelMesh> background_mesh;
public:
	glm::vec3 text_color = {};
	glm::vec3 background_color = {};
	float scale = 1.f;
public:
	Label();
public:
	//void format(const std::string &text, texture_font_t *font);
};

class LabelFont {
public:
	LabelFont(const std::string &fontpath, size_t fontsize);
	~LabelFont();
};

};
