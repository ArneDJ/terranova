#include <GL/glew.h>
#include <glm/glm.hpp>
#include "../util/logger.h"
#include "font.h"

void FontManager::init()
{
	if (FT_Init_FreeType(&m_ft_library)) {
		logger::ERROR("Could not init FreeType Library");
	}

	std::string filepath = "fonts/arial.ttf";
	if (FT_New_Face(m_ft_library, filepath.c_str(), 0, &m_face)) {
		logger::ERROR("Failed to load font {}", filepath);
	}

	FT_Set_Pixel_Sizes(m_face, 0, 48);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

	// generate font texture
	// TODO store in single texture
	for (unsigned char c = 0; c < 128; c++) {
		// load character glyph
		if (FT_Load_Char(m_face, c, FT_LOAD_RENDER)) {
			logger::ERROR("failed to load glyph {}", c);
			continue;
		}
		// generate texture
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, m_face->glyph->bitmap.buffer);
		// set texture options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// now store character for later use
		Character character = {
			texture,
			glm::ivec2(m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows),
			glm::ivec2(m_face->glyph->bitmap_left, m_face->glyph->bitmap_top),
			m_face->glyph->advance.x
		};
		m_characters[c] = character;
	}

	// generate the font mesh
	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);
	glBindVertexArray(m_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}
	
void FontManager::shutdown()
{
	FT_Done_Face(m_face);
	FT_Done_FreeType(m_ft_library);

	for (auto& [key, character] : m_characters) {
		glDeleteTextures(1, &character.texture_id);
	}
}
	
void FontManager::display_text(const std::string& text, float x, float y, float scale)
{
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(m_VAO);

	// find the horizontal offset
	float label_width = 0.f;
	for (const auto& [key, character] : m_characters) {
		label_width += character.bearing.x * scale;
	}
	
	x -= 0.25f * label_width;

	// iterate through all characters
	std::string::const_iterator c;
	for (c = text.begin(); c != text.end(); c++) {
		Character character = m_characters[*c];

		float xpos = x + character.bearing.x * scale;
		float ypos = y - (character.size.y - character.bearing.y) * scale;

		float w = character.size.x * scale;
		float h = character.size.y * scale;
		// update VBO for each character
		float vertices[6][4] = {
		{ xpos,     ypos + h,   0.0f, 0.0f },
		{ xpos,     ypos,       0.0f, 1.0f },
		{ xpos + w, ypos,       1.0f, 1.0f },

		{ xpos,     ypos + h,   0.0f, 0.0f },
		{ xpos + w, ypos,       1.0f, 1.0f },
		{ xpos + w, ypos + h,   1.0f, 0.0f }
		};
		// render glyph texture over quad
		glBindTexture(GL_TEXTURE_2D, character.texture_id);
		// update content of VBO memory
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		// render quad
		glDrawArrays(GL_TRIANGLES, 0, 6);
		// now advance cursors for next glyph (note that advance is number of 1/64 pixels)
		x += (character.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
	}

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
}
