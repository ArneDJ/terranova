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
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "../extern/loguru/loguru.hpp"

#include "../util/camera.h"
#include "../util/image.h"
#include "../util/animation.h"
#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../graphics/shader.h"
#include "../graphics/mesh.h"
#include "../graphics/texture.h"
#include "../graphics/model.h"
#include "../physics/physical.h"
#include "../physics/bumper.h"

#include "creature.h"

static const float STANDARD_CAPSULE_RADIUS = 0.4F;
static const float STANDARD_CAPSULE_HEIGHT = 2.F;

static const std::vector<HitBoxInput> input_hitboxes = {
	{ "spine.004", "spine.005", 0.1f, 0.3f },
	{ "spine.001", "spine.003", 0.2f, 0.4f },
	{ "upper_arm.R", "forearm.R", 0.08f, 0.3f },
	{ "upper_arm.L", "forearm.L", 0.08f, 0.3f },
	{ "forearm.R", "hand.R", 0.08f, 0.3f },
	{ "forearm.L", "hand.L", 0.08f, 0.3f },
	{ "thigh.R", "shin.R", 0.1f, 0.3f },
	{ "thigh.L", "shin.L", 0.1f, 0.3f },
	{ "shin.R", "foot.R", 0.1f, 0.4f },
	{ "shin.L", "foot.L", 0.1f, 0.4f }
};

static inline glm::quat direction_to_quat(glm::vec2 direction)
{
	float angle = atan2(direction.x, direction.y);
	glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.f, 1.f, 0.f));

	return rotation;
}

Creature::Creature()
{
	bumper = std::make_unique<fysx::Bumper>(glm::vec3(0.f), STANDARD_CAPSULE_RADIUS, STANDARD_CAPSULE_HEIGHT);

	transform = std::make_unique<geom::Transform>();
	
	joint_matrices.buffer.set_target(GL_SHADER_STORAGE_BUFFER);
}
	
void Creature::set_animation(util::AnimationSet *set)
{
	anim_set = set;

	character_animation.locals.resize(set->skeleton->num_soa_joints());
	character_animation.models.resize(set->skeleton->num_joints());
	character_animation.cache.Resize(set->max_tracks);

	joint_matrices.data.resize(set->skeleton->num_joints());
	joint_matrices.update_present();

	// find attachments
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "eyes")) {
			skeleton_attachments.eyes = i;
			break;
		}
	}	
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "spine.002")) {
			skeleton_attachments.spine = i;
			break;
		}
	}	
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "spine.005")) {
			skeleton_attachments.head = i;
			break;
		}
	}	
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "upper_arm.R")) {
			skeleton_attachments.upper_right_arm = i;
			break;
		}
	}	
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "forearm.R")) {
			skeleton_attachments.lower_right_arm = i;
			break;
		}
	}	

	for (const auto &input : input_hitboxes) {
		int joint_a = -1;
		int joint_b = -1;
		for (int i = 0; i < set->skeleton->num_joints(); i++) {
			if (std::strstr(set->skeleton->joint_names()[i], input.joint_a.c_str())) {
				joint_a = i;
				break;
			}
		}	
		for (int i = 0; i < set->skeleton->num_joints(); i++) {
			if (std::strstr(set->skeleton->joint_names()[i], input.joint_b.c_str())) {
				joint_b = i;
				break;
			}
		}	

		if (joint_a >= 0 && joint_b >= 0) {
			auto hitbox = std::make_unique<HitBox>(input.radius, input.half_height);
			hitbox->joint_target_a = joint_a;
			hitbox->joint_target_b = joint_b;
			hitboxes.push_back(std::move(hitbox));
		}
	}
}
	
void Creature::set_movement(const glm::vec3 &direction, bool jump_request)
{
	if (direction != glm::vec3(0, 0, 0)) {
		bumper->walk_direction = glm::normalize(direction);
	} else {
		bumper->walk_direction = glm::vec3(0.f);
	}
}
	
void Creature::teleport(const glm::vec3 &position)
{
	bumper->teleport(position);
}
	
void Creature::update_transform()
{
	bumper->sync_transform();

	transform->position = bumper->transform->position;
	transform->position.y -= (bumper->shape->getHalfHeight() + bumper->shape->getRadius());

	// set rotation
	if (bumper->walk_direction.x != 0.f || bumper->walk_direction.z != 0.f) {
		glm::vec2 d = { bumper->walk_direction.x, bumper->walk_direction.z };
		transform->rotation = direction_to_quat(glm::normalize(d));
	}

	// select animation
	if (!bumper->on_ground) {
		current_animation = CA_FALLING;
	} else if (bumper->walk_direction.x != 0.f || bumper->walk_direction.z != 0.f) {
		current_animation = CA_RUN;
	} else {
		current_animation = CA_IDLE;
	}
}
	
void Creature::update_animation(float delta)
{
	const ozz::animation::Animation *animation = anim_set->animations[current_animation];

	if (update_character_animation(&character_animation, animation, anim_set->skeleton, delta)) {
		for (const auto &skin : model->skins()) {
			if (skin->inverse_binds.size() == character_animation.models.size()) {
				for (int i = 0; i < character_animation.models.size(); i++) {
					joint_matrices.data[i] = util::ozz_to_mat4(character_animation.models[i]) * skin->inverse_binds[i];
				}
				break; // only animate first skin
			}
		}
		joint_matrices.update_present();
	}

	// find eye position
	// Prepares attached object transformation.
	// Gets model space transformation of the joint.
	if (skeleton_attachments.eyes >= 0) {
		const glm::mat4 &joint = util::ozz_to_mat4(character_animation.models[skeleton_attachments.eyes]);

		glm::mat4 translation = transform->to_matrix() * joint;

		eye_position.x = translation[3][0];
		eye_position.y = translation[3][1];
		eye_position.z = translation[3][2];
	}
}
	
void Creature::update_hitboxes()
{
	for (auto &hitbox : hitboxes) {
		const glm::mat4 &joint_a = util::ozz_to_mat4(character_animation.models[hitbox->joint_target_a]);
		glm::mat4 translation_a = transform->to_matrix() * joint_a;
		glm::vec3 position_a = { translation_a[3][0], translation_a[3][1], translation_a[3][2] };

		const glm::mat4 &joint_b = util::ozz_to_mat4(character_animation.models[hitbox->joint_target_b]);
		glm::mat4 translation_b = transform->to_matrix() * joint_b;
		glm::vec3 position_b = { translation_b[3][0], translation_b[3][1], translation_b[3][2] };

		glm::vec3 position = geom::midpoint(position_a, position_b);
		glm::vec3 direction = glm::normalize(position_a - position_b);

		glm::mat4 rotmat = glm::orientation(direction, glm::vec3(0.f, 1.f, 0.f));
		glm::quat rotation = glm::quat(rotmat);

		hitbox->set_transform(position, rotation);
	}
}
	
void Creature::update_collision(const btDynamicsWorld *world, float delta)
{
	bumper->update(world, delta);
}

void Creature::set_scale(float scale)
{
	transform->scale.x = scale;
	transform->scale.y = scale;
	transform->scale.z = scale;

	bumper->set_scale(scale);

	for (auto &hitbox : hitboxes) {
		hitbox->set_scale(scale);
	}
}
