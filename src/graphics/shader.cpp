#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../util/logger.h"
#include "shader.h"

namespace gfx {

Shader::Shader()
{
	m_program = glCreateProgram();
}

Shader::~Shader()
{
	// detach and delete shader objects
	for (GLuint object : m_shaders) {
		if (glIsShader(object) == GL_TRUE) {
			glDetachShader(m_program, object);
			glDeleteShader(object);
		}
	}

	// delete shader program
	if (glIsProgram(m_program) == GL_TRUE) { glDeleteProgram(m_program); }
}

void Shader::compile(const std::string &filepath, GLenum type)
{
	std::ifstream file(filepath);
        if (file.fail()) {
		logger::ERROR("Shader compile error: failed to open {}", filepath);
		return;
        }

	// read source file contents into memory
	std::string buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	file.close();

	// now create the OpenGL shader
	const GLchar *source = buffer.c_str();

	compile_source(source, type);
}
	
void Shader::compile_source(const GLchar *source, GLenum type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);

	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled == GL_FALSE) {
		// give the error
		GLsizei len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		std::vector<GLchar> log(len);
		glGetShaderInfoLog(shader, len, &len, log.data());
		logger::ERROR("Compilation failed: {}", log.data());

		// clean up shader
		glDeleteShader(shader);

		return;
	}

	m_shaders.push_back(shader);
}

void Shader::link()
{
	// attach the shaders to the shader program
	for (GLuint object : m_shaders) {
		if (glIsShader(object) == GL_TRUE) {
			glAttachShader(m_program, object);
		}
	}

	glLinkProgram(m_program);

	// check if shader program was linked
	GLint linked;
	glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLsizei len;
		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
		std::vector<GLchar> log(len);
		glGetProgramInfoLog(m_program, len, &len, log.data());
		logger::ERROR("Program linking failed: {}", log.data());

		for (GLuint object : m_shaders) {
			glDetachShader(m_program, object);
			glDeleteShader(object);
		}
	}
}

void Shader::use() const
{
	glUseProgram(m_program);
}

void Shader::uniform_bool(const GLchar *name, bool boolean) const
{
	glUniform1i(glGetUniformLocation(m_program, name), boolean);
}

void Shader::uniform_int(const GLchar *name, int integer) const
{
	glUniform1i(glGetUniformLocation(m_program, name), integer);
}

void Shader::uniform_float(const GLchar *name, GLfloat scalar) const
{
	glUniform1f(glGetUniformLocation(m_program, name), scalar);
}

void Shader::uniform_vec2(const GLchar *name, glm::vec2 vector) const
{
	glUniform2fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(vector));
}

void Shader::uniform_vec3(const GLchar *name, glm::vec3 vector) const
{
	glUniform3fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(vector));
}

void Shader::uniform_vec4(const GLchar *name, glm::vec4 vector) const
{
	glUniform4fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(vector));
}

void Shader::uniform_mat4(const GLchar *name, glm::mat4 matrix) const
{
	glUniformMatrix4fv(glGetUniformLocation(m_program, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::uniform_mat4_array(const GLchar *name, const std::vector<glm::mat4> &matrices) const
{
	glUniformMatrix4fv(glGetUniformLocation(m_program, name), matrices.size(), GL_FALSE, glm::value_ptr(matrices[0]));
}

GLint Shader::uniform_location(const GLchar *name) const
{
	return glGetUniformLocation(m_program, name);
}
	
GLuint Shader::uniform_block_index(const GLchar *name) const
{
	return glGetUniformBlockIndex(m_program, name);
}
	
GLuint Shader::resource_index(GLenum interface, const GLchar *name) const
{
	return glGetProgramResourceIndex(m_program, interface, name);
}
	
void Shader::bind_uniform_block(GLuint index, GLuint binding) const
{
	glUniformBlockBinding(m_program, index, binding);
}
	
void Shader::bind_storage_block(GLuint index, GLuint binding) const
{
	glShaderStorageBlockBinding(m_program, index, binding);
}

void Shader::set_storage_block(const GLchar *name, GLuint binding) const
{
	GLuint index = resource_index(GL_SHADER_STORAGE_BLOCK, name);

	if (index == GL_INVALID_INDEX) {
		//logger::(ERROR, "Shader storage block binding error: invalid index for %s", name);
	}

	bind_storage_block(index, binding);
}

void Shader::set_uniform_block(const GLchar *name, GLuint binding) const
{
	GLuint index = uniform_block_index(name);

	if (index == GL_INVALID_INDEX) {
		//logger::(ERROR, "Shader uniform block binding error: invalid index for %s", name);
	}

	bind_uniform_block(index, binding);
}

};
