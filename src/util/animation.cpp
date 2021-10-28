#include <iostream>
#include <vector>
#include <map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"

#include "animation.h"

namespace util {

void PlaybackController::update(const ozz::animation::Animation *animation, float dt)
{
	float new_time = time_ratio;

	if (m_playing) {
		new_time = time_ratio + dt * m_playback_speed / animation->duration();
	}

	// Must be called even if time doesn't change, in order to update previous
	// frame time ratio. Uses set_time_ratio function in order to update
	// previous_time_ an wrap time value in the unit interval (depending on loop
	// mode).
	set_time_ratio(new_time);
}

void PlaybackController::set_time_ratio(float ratio)
{
	m_previous_time_ratio = time_ratio;
	if (m_looping) {
		// Wraps in the unit interval [0:1], even for negative values (the reason
		// for using floorf).
		time_ratio = ratio - floorf(ratio);
	} else {
		// Clamps in the unit interval [0:1].
		time_ratio = glm::clamp(ratio, 0.f, 1.f);
	}
}

void PlaybackController::reset()
{
	m_previous_time_ratio = time_ratio = 0.f;
	m_playback_speed = 1.f;
	m_playing = true;
}

bool update_character_animation(CharacterAnimation *character, const ozz::animation::Animation *animation, const ozz::animation::Skeleton *skeleton, float dt)
{
	// Samples animation.
	character->controller.update(animation, dt);

	// Setup sampling job.
	ozz::animation::SamplingJob sampling_job;
	sampling_job.animation = animation;
	sampling_job.cache = &character->cache;
	sampling_job.ratio = character->controller.time_ratio;
	sampling_job.output = ozz::make_span(character->locals);

	// Samples animation.
	if (!sampling_job.Run()) { return false; }

	// Converts from local space to model space matrices.
	ozz::animation::LocalToModelJob ltm_job;
	ltm_job.skeleton = skeleton;
	ltm_job.input = ozz::make_span(character->locals);
	ltm_job.output = ozz::make_span(character->models);

	if (!ltm_job.Run()) { return false; }

	return true;
}

};
