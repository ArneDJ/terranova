namespace gpu {

class Shader {
public:
	Shader();
	~Shader();
public:
	void compile(const std::string &filepath, GLenum type);
	void compile_source(const GLchar *source, GLenum type);
	void link();
	void use() const;
public:
	void uniform_bool(const GLchar *name, bool boolean) const;
	void uniform_int(const GLchar *name, int integer) const;
	void uniform_float(const GLchar *name, GLfloat scalar) const;
	void uniform_vec2(const GLchar *name, glm::vec2 vector) const;
	void uniform_vec3(const GLchar *name, glm::vec3 vector) const;
	void uniform_vec4(const GLchar *name, glm::vec4 vector) const;
	void uniform_mat4(const GLchar *name, glm::mat4 matrix) const;
	void uniform_mat4_array(const GLchar *name, const std::vector<glm::mat4> &matrices) const;
	GLint uniform_location(const GLchar *name) const;
private:
	GLuint program = 0;
	std::vector<GLuint> shaders;
};

};
