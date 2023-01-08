#pragma once
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

struct Character {
	unsigned int texture_id = 0;  // ID handle of the glyph texture
	glm::ivec2 size = {};       // Size of glyph
	glm::ivec2 bearing = {};    // Offset from baseline to left/top of glyph
	long int advance = 0;    // Offset to advance to next glyph
};

class FontManager {
public:
	void init();
	void shutdown();
	void display_text(const std::string& text, float x, float y, float scale);
private:
	FT_Library m_ft_library;
	FT_Face m_face;
	std::map<char, Character> m_characters;
	GLuint m_VAO = 0;
	GLuint m_VBO = 0;
};
