#include <iostream>
#include <vector>
#include <unordered_map>
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

bool update_blended_animation(CharacterAnimation *sampler_a, CharacterAnimation *sampler_b, const ozz::animation::Animation *animation_a, const ozz::animation::Animation *animation_b, const ozz::animation::Skeleton *skeleton, float dt)
{
	// Samples animation.
	sampler_a->controller.update(animation_a, dt);
	// Setup sampling job.
	if (sampler_a->weight > 0.f) {
		ozz::animation::SamplingJob sampling_job;
		sampling_job.animation = animation_a;
		sampling_job.cache = &sampler_a->cache;
		sampling_job.ratio = sampler_a->controller.time_ratio;
		sampling_job.output = ozz::make_span(sampler_a->locals);
		// Samples animation.
		if (!sampling_job.Run()) { return false; }
	}
	// Samples animation.
	sampler_b->controller.update(animation_b, dt);
	// Setup sampling job.
	if (sampler_b->weight > 0.f) {
		ozz::animation::SamplingJob sampling_job;
		sampling_job.animation = animation_b;
		sampling_job.cache = &sampler_b->cache;
		sampling_job.ratio = sampler_b->controller.time_ratio;
		sampling_job.output = ozz::make_span(sampler_b->locals);
		// Samples animation.
		if (!sampling_job.Run()) { return false; }
	}

	// Blends animations.
	// Blends the local spaces transforms computed by sampling all animations
	// (1st stage just above), and outputs the result to the local space
	// transform buffer blended_locals_

	// Prepares blending layers.
	std::vector<ozz::animation::BlendingJob::Layer> layers;
	{
		ozz::animation::BlendingJob::Layer layer;
		layer.transform = ozz::make_span(sampler_a->locals);
		layer.weight = sampler_a->weight;
		layers.push_back(layer);
	}
	{
		ozz::animation::BlendingJob::Layer layer;
		layer.transform = ozz::make_span(sampler_b->locals);
		layer.weight = sampler_b->weight;
		layers.push_back(layer);
	}
	// Blending job bind pose threshold.
	float threshold = 0.1f;

	// Setups blending job.
	ozz::animation::BlendingJob blend_job;
	blend_job.threshold = threshold;
	blend_job.layers = ozz::make_span(layers);
	blend_job.bind_pose = skeleton->joint_bind_poses();
	blend_job.output = ozz::make_span(sampler_a->blended_locals);

	// Blends.
	if (!blend_job.Run()) { return false; }

	// Converts from local space to model space matrices.
	ozz::animation::LocalToModelJob ltm_job;
	ltm_job.skeleton = skeleton;
	ltm_job.input = ozz::make_span(sampler_a->blended_locals);
	ltm_job.output = ozz::make_span(sampler_a->models);

	if (!ltm_job.Run()) { return false; }

	return true;
}
	
void AnimationSet::find_max_tracks()
{
	max_tracks = 0;

	for (auto it = animations.begin(); it != animations.end(); it++) {
		auto anim = it->second;
		if (anim->num_tracks() > max_tracks) {
			max_tracks =anim->num_tracks();
		}
	}
}
	
AnimationController::AnimationController(const AnimationSet *set)
	: m_set(set)
{
	// find max tracks
	locals.resize(set->skeleton->num_soa_joints());
	models.resize(set->skeleton->num_joints());
	cache.Resize(set->max_tracks);
}
	
void AnimationController::update(uint8_t animation_id, float delta)
{
	// find if animation exist
	// TODO keep track of prev animation so we don't need to a search every time
	auto search = m_set->animations.find(animation_id);
	if (search != m_set->animations.end()) {
		auto &animation = search->second;
		// Samples animation.
		playback.update(animation, delta);

		// Setup sampling job.
		ozz::animation::SamplingJob sampling_job;
		sampling_job.animation = animation;
		sampling_job.cache = &cache;
		sampling_job.ratio = playback.time_ratio;
		sampling_job.output = ozz::make_span(locals);

		// Samples animation.
		if (!sampling_job.Run()) { return; }

		// Converts from local space to model space matrices.
		ozz::animation::LocalToModelJob ltm_job;
		ltm_job.skeleton = m_set->skeleton;
		ltm_job.input = ozz::make_span(locals);
		ltm_job.output = ozz::make_span(models);

		ltm_job.Run();
	}
}

};
