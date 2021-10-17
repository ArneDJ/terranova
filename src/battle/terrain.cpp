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

#include "../extern/fastnoise/FastNoise.h"

#include "../util/camera.h"
#include "../util/image.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/texture.h"
#include "../graphics/model.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "terrain.h"

Terrain::Terrain(std::shared_ptr<gfx::Shader> shader)
	: m_shader(shader)
{
	geom::Rectangle bounds = { { 0.F, 0.F }, { m_scale.x, m_scale.z } };
	m_mesh.create(32, bounds);

	m_heightmap.resize(512, 512, util::COLORSPACE_GRAYSCALE);

	m_texture.create(m_heightmap);
	
	m_height_field = std::make_unique<fysx::HeightField>(m_heightmap, m_scale);
}
	
void Terrain::generate(int seed) 
{
	FastNoise fastnoise;
	fastnoise.SetSeed(seed);
	fastnoise.SetNoiseType(FastNoise::SimplexFractal);
	fastnoise.SetFractalType(FastNoise::FBM);
	fastnoise.SetFrequency(0.005f);
	fastnoise.SetFractalOctaves(6);
	fastnoise.SetFractalLacunarity(2.5f);
	fastnoise.SetPerturbFrequency(0.001f);
	fastnoise.SetGradientPerturbAmp(20.f);

	for (int i = 0; i < m_heightmap.width(); i++) {
		for (int j = 0; j < m_heightmap.height(); j++) {
			float x = i; float y = j;
			fastnoise.GradientPerturbFractal(x, y);
			float height = 0.5f * (fastnoise.GetNoise(x, y) + 1.f);
			m_heightmap.plot(i, j, util::CHANNEL_RED, 255 * height);
		}
	}

	// store heightmap in texture
	m_texture.reload(m_heightmap);
}
	
void Terrain::display(const util::Camera &camera) const
{
	m_shader->use();
	m_shader->uniform_mat4("CAMERA_VP", camera.VP);
	m_shader->uniform_vec3("MAP_SCALE", m_scale);

	m_texture.bind(GL_TEXTURE0);

	m_mesh.draw();
}

fysx::HeightField* Terrain::height_field()
{
	return m_height_field.get();
}
	
