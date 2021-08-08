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

#include "geometry/transform.h"
#include "graphics/mesh.h"
#include "graphics/model.h"

#include "media.h"

std::map<uint64_t, std::unique_ptr<gfx::Model>> MediaManager::m_models;

const gfx::Model* MediaManager::load_model(const std::string &filepath)
{
	//std::string filepath = m_directory + "models/" + path;
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
