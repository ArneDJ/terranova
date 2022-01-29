#include "../extern/fastnoise/FastNoise.h"

#include "image.h"
#include "noise.h"

namespace util {

void noise_image(Image<float> &image, FastNoise *fastnoise, const glm::vec2 &sample_freq, uint8_t channel)
{
	const int nsteps = 32;
	const int stepsize = image.width() / nsteps;

	#pragma omp parallel for
	for (int step_x = 0; step_x < image.width(); step_x += stepsize) {
		int w = step_x + stepsize;
		int h = image.height();
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				float x = sample_freq.x * j;
				float y = sample_freq.y * i;
				fastnoise->GradientPerturbFractal(x, y);
				float value = 0.5f * (fastnoise->GetNoise(x, y) + 1.f);
				image.plot(j, i, channel, glm::clamp(value, 0.f, 1.f));
			}
		}
	}
}

};
