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
#include "../graphics/model.h"
#include "../physics/physical.h"
#include "../physics/heightfield.h"

#include "terrain.h"

Terrain::Terrain(const gfx::Shader *shader)
	: m_shader(shader)
{
	m_mesh.create(32, m_bounds);

	m_heightmap.resize(512, 512, util::COLORSPACE_GRAYSCALE);
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
}
	
void Terrain::display(const util::Camera &camera) const
{
	m_shader->use();
	m_shader->uniform_mat4("CAMERA_VP", camera.VP);

	m_mesh.draw();
}
