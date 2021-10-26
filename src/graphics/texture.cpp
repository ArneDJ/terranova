#include <vector>
#include <fstream>
#include <array>
#include <algorithm>
#include <memory>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

#define DDSKTX_IMPLEMENT
#include "../extern/ddsktx/dds-ktx.h"

#include "../util/image.h"

#include "texture.h"

namespace gfx {

static inline GLenum dds_texture_format(ddsktx_format format);

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
	
void Texture::load_dds(const uint8_t *blob, const size_t size)
{
	ddsktx_texture_info tc = { 0 };

	ddsktx_error error;
	if (ddsktx_parse(&tc, blob, size, &error)) {
		m_format = dds_texture_format(tc.format);
		if (!m_format) {
			LOG_F(ERROR, "DDS error: Invalid texture format");
			return;
		}

		// Create GPU texture from tc data
		glBindTexture(GL_TEXTURE_2D, m_binding);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, tc.num_mips-1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		for (int mip = 0; mip < tc.num_mips; mip++) {
			ddsktx_sub_data sub_data;
			ddsktx_get_sub(&tc, &sub_data, blob, size, 0, 0, mip);
			if (sub_data.width <= 4 || sub_data.height <= 4) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip-1);
				break;
			}
			// Fill/Set texture sub resource data (mips in this case)
			glCompressedTexImage2D(GL_TEXTURE_2D, mip, m_format, sub_data.width, sub_data.height, 0, sub_data.size_bytes, sub_data.buff);
		}
	} else {
		std::string err = error.msg;
		LOG_F(ERROR, "DDS error: %s", err.c_str());
	}
}

static inline GLenum dds_texture_format(ddsktx_format format)
{
	switch (format) {
	case DDSKTX_FORMAT_BC1: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; // DXT1
	case DDSKTX_FORMAT_BC2: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; // DXT3
	case DDSKTX_FORMAT_BC3: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // DXT5
	// case DDSKTX_FORMAT_ETC1: return GL_ETC1_RGB8_OES;       // ETC1 RGB8 not recognized on Windows
	// case DDSKTX_FORMAT_ETC2: return GL_COMPRESSED_RGB8_ETC2;      // ETC2 RGB8 not recognized on Windows
	default : return 0;
	}
}

};
