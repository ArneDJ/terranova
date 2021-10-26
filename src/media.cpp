#include <string>
#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/loguru/loguru.hpp"

#include "util/image.h"
#include "geometry/transform.h"
#include "graphics/mesh.h"
#include "graphics/texture.h"
#include "graphics/model.h"

#include "media.h"

std::map<uint64_t, std::unique_ptr<gfx::Model>> MediaManager::m_models;
std::map<uint64_t, std::unique_ptr<gfx::Texture>> MediaManager::m_textures;

const gfx::Model* MediaManager::load_model(const std::string &filepath)
{
	uint64_t key = std::hash<std::string>()(filepath);

	auto search = m_models.find(key);
	// model not found in map
	// load new one
	if (search == m_models.end()) {
		m_models[key] = std::make_unique<gfx::Model>(filepath);
		
		return m_models[key].get();
	}

	return search->second.get();
}
	
const gfx::Texture* MediaManager::load_texture(const std::string &filepath)
{
	uint64_t key = std::hash<std::string>()(filepath);

	auto search = m_textures.find(key);
	// texture not found in map
	// load new one
	if (search == m_textures.end()) {
		auto texture = std::make_unique<gfx::Texture>();
		// import file blob
		FILE *file = fopen(filepath.c_str(), "rb");
		if (!file) {
			LOG_F(ERROR, "texture load error: failed to open file %s", filepath.c_str());
		} else {
			fseek(file, 0L, SEEK_END);
			size_t size = ftell(file);
			rewind(file);

			uint8_t *buffer = new uint8_t[size];
			fread(buffer, sizeof *buffer, size, file);

			fclose(file);

			texture->load_dds(buffer, size);

			delete [] buffer;
		}
		
		m_textures[key] = std::move(texture);

		return m_textures[key].get();
	}

	return search->second.get();
}
