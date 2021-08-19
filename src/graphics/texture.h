namespace gfx {

class Texture {
public:
	Texture();
	~Texture();
public:
	void create(const util::Image<uint8_t> &image);
	void reload(const util::Image<uint8_t> &image);
	void bind(GLenum unit) const;
private:
	GLuint m_binding = 0;
	GLenum m_target = GL_TEXTURE_2D;
	GLenum m_format = GL_RED;
};

};
