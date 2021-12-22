#include <iostream>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/fastgaussianblur/fast_gaussian_blur.h"
#include "../extern/fastgaussianblur/fast_gaussian_blur_template.h"

#include "../geometry/geometry.h"
#include "image.h"

namespace util {

template<>
void Image<uint8_t>::blur(float sigma)
{
	uint8_t *blurred = new uint8_t[m_raster.size()];
	uint8_t *input = m_raster.data();

	fast_gaussian_blur_template(input, blurred, m_width, m_height, m_channels, sigma);

	std::swap(input, blurred);

	delete [] blurred;
}

template<>
void Image<float>::blur(float sigma)
{
	float *blurred = new float[m_raster.size()];
	float *input = m_raster.data();

	fast_gaussian_blur_template(input, blurred, m_width, m_height, m_channels, sigma);

	std::swap(input, blurred);

	delete [] blurred;
}

template<>
void Image<float>::normalize(uint8_t channel)
{
	const int nsteps = 32;
	const int stepsize = m_width / nsteps;

	// find min and max
	float min = (std::numeric_limits<float>::max)();
	float max = (std::numeric_limits<float>::min)();
	for (int x = 0; x < m_width; x++) {
		for (int y = 0; y < m_height; y++) {
			float value = sample(x, y, channel);
			min = (std::min)(min, value);
			max = (std::max)(max, value);
		}
	}

	float scale = max - min;

	if (scale == 0.f) {
		return;
	}

	// now normalize the values
	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += stepsize) {
		int w = step_x + stepsize;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float value = sample(j, i, channel);
				value = glm::clamp((value - min) / scale, 0.f, 1.f);
				plot(j, i, channel, value);
			}
		}
	}
}

};
