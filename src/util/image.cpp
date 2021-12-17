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

};
