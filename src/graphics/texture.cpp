#include <vector>
#include <array>
#include <algorithm>
#include <memory>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../util/image.h"

#include "texture.h"

namespace gfx {

Texture::Texture()
{
	glGenTextures(1, &m_binding);
}

Texture::~Texture()
{
	if (glIsTexture(m_binding) == GL_TRUE) {
		glDeleteTextures(1, &m_binding);
	}
}

// TODO make more general purpose
void Texture::create(const util::Image<uint8_t> &image)
{
	m_target = GL_TEXTURE_2D;

	GLenum internal_format = 0;
	GLenum type = GL_UNSIGNED_BYTE;

	switch (image.channels()) {
	case 1:
		internal_format = GL_R8;
		m_format = GL_RED;
		break;
	case 2:
		internal_format = GL_RG8;
		m_format = GL_RG;
		break;
	case 3:
		internal_format = GL_RGB8;
		m_format = GL_RGB;
		break;
	case 4:
		internal_format = GL_RGBA8;
		m_format = GL_RGBA;
		break;
	}

	glBindTexture(m_target, m_binding);

	glTexStorage2D(m_target, 1, internal_format, image.width(), image.height());
	glTexSubImage2D(m_target, 0, 0, 0, image.width(), image.height(), m_format, type, image.raster().data());

	glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(m_target, 0);
}

void Texture::reload(const util::Image<uint8_t> &image)
{
	glBindTexture(m_target, m_binding);
	glTexSubImage2D(m_target, 0, 0, 0, image.width(), image.height(), m_format, GL_UNSIGNED_BYTE, image.raster().data());
}

void Texture::bind(GLenum unit) const
{
	glActiveTexture(unit);
	glBindTexture(m_target, m_binding);
}

};
