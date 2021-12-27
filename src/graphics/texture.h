namespace gfx {

class Texture {
public:
	Texture();
	~Texture();
public:
	void create(const util::Image<uint8_t> &image);
	void reload(const util::Image<uint8_t> &image);
	void create(const util::Image<float> &image);
	void reload(const util::Image<float> &image);
	void load_dds(const uint8_t *blob, const size_t size);
	void change_filtering(GLint filter);
public:
	void bind(GLenum unit) const;
public:
	GLuint binding();
private:
	GLuint m_binding = 0;
	GLenum m_target = GL_TEXTURE_2D;
	GLenum m_format = GL_RED;
};

};
