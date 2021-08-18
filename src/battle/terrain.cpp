#include <iostream>
#include <chrono>
#include <unordered_map>
#include <list>
#include <memory>
#include <random>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/loguru/loguru.hpp"

#include "../util/camera.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "terrain.h"

Terrain::Terrain(const gfx::Shader *shader)
	: m_shader(shader)
{
	m_mesh.create(32, m_bounds);
}
	
void Terrain::display(const util::Camera &camera) const
{
	m_shader->use();
	m_shader->uniform_mat4("CAMERA_VP", camera.VP);

	m_mesh.draw();
}
