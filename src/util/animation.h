#pragma once
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

namespace util {

class PlaybackController {
public:
	// Current animation time ratio, in the unit interval [0,1], where 0 is the
	// beginning of the animation, 1 is the end.
	float time_ratio = 0.f;
public:
	// Sets animation current time.
	void set_time_ratio(float ratio);
	// Sets loop modes. If true, animation time is always clamped between 0 and 1.
	void set_looping(bool loop) { m_looping = loop; }

	void set_speed(float speed) { m_playback_speed = speed; }
	// Gets loop mode.
	bool looping() const { return m_looping; }
	// Updates animation time if in "play" state, according to playback speed and
	// given frame time _dt.
	// Returns true if animation has looped during update
	void update(const ozz::animation::Animation *animation, float _dt);
	// Resets all parameters to their default value.
	void reset();
	// Do controller Gui.
	// Returns true if animation time has been changed.
	//bool OnGui(const ozz::animation::Animation& _animation, ImGui* _im_gui,
	//bool _enabled = true, bool _allow_set_time = true);
private:
	// Time ratio of the previous update.
	float m_previous_time_ratio = 0.f;
	// Playback speed, can be negative in order to play the animation backward.
	float m_playback_speed = 1.f;
	// Animation play mode state: play/pause.
	bool m_playing = true;
	// Animation loop mode.
	bool m_looping = true;
};

// contains all the data required to sample and blend a character animation
// this is unique for every animated character
struct CharacterAnimation {
	std::vector<ozz::math::SoaTransform> blended_locals; // TODO make unique blend output
	float weight = 1.f; // blend weight
	PlaybackController controller;
	ozz::animation::SamplingJob::Context context;
	ozz::vector<ozz::math::SoaTransform> locals; // Buffer of local transforms which stores the blending result.
 	ozz::vector<ozz::math::Float4x4> models; // Buffer of model space matrices. These are computed by the local-to-model // job after the blending stage.
};

bool update_character_animation(CharacterAnimation *character, const ozz::animation::Animation *animation, const ozz::animation::Skeleton *skeleton, float dt);

bool update_blended_animation(CharacterAnimation *sampler_a, CharacterAnimation *sampler_b, const ozz::animation::Animation *animation_a, const ozz::animation::Animation *animation_b, const ozz::animation::Skeleton *skeleton, float dt);

// this is shared among animated characters
class AnimationSet {
public:
	const ozz::animation::Skeleton *skeleton = nullptr;
	std::unordered_map<uint8_t, const ozz::animation::Animation*> animations; // left action, right animation
	int max_tracks = 0;
public:
	void find_max_tracks();
};

class AnimationController {
public:
	PlaybackController playback;
	ozz::animation::SamplingJob::Context context;
	ozz::vector<ozz::math::SoaTransform> locals; // Buffer of local transforms which stores the blending result.
 	ozz::vector<ozz::math::Float4x4> models; // Buffer of model space matrices. These are computed by the local-to-model // job after the blending stage.
public:
	AnimationController(const AnimationSet *set);
public:
	void update(uint8_t animation_id, float delta);
private:
	const AnimationSet *m_set = nullptr;
};

inline glm::mat4 ozz_to_mat4(const ozz::math::Float4x4 &matrix)
{
	glm::mat4 out;
	ozz::math::StorePtrU(matrix.cols[0], glm::value_ptr(out));
	ozz::math::StorePtrU(matrix.cols[1], glm::value_ptr(out) + 4);
	ozz::math::StorePtrU(matrix.cols[2], glm::value_ptr(out) + 8);
	ozz::math::StorePtrU(matrix.cols[3], glm::value_ptr(out) + 12);

	return out;
}

};
