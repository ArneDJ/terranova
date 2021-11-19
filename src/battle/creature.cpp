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

float FBX_SCALING_FACTOR = 0.01F; // fbx models downloaded from mixamo scales to 100 for some dumb reason

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
	model_transform = std::make_unique<geom::Transform>();
	model_transform->scale = glm::vec3(0.01f);
	
	joint_matrices.buffer.set_target(GL_SHADER_STORAGE_BUFFER);

	root_hitbox = std::make_unique<HitBoxRoot>(1.1F * (STANDARD_CAPSULE_HEIGHT));
	root_hitbox->ghost_object->setUserPointer(this);
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
		if (std::strstr(set->skeleton->joint_names()[i], "HeadTop_End")) {
			skeleton_attachments.eyes = i;
			break;
		}
	}	
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "RightHandIndex1")) {
			skeleton_attachments.right_hand = i;
			break;
		}
	}	
	for (int i = 0; i < set->skeleton->num_joints(); i++) {
		if (std::strstr(set->skeleton->joint_names()[i], "hand.L")) {
			skeleton_attachments.left_hand = i;
			break;
		}
	}	
}
	
// add a copy of a hit capsule so this unique creature entity can scale it to its unique scale 
void Creature::set_hitbox(const std::vector<HitCapsule> &capsules)
{
	hitboxes = capsules;
}
	
void Creature::set_movement(const glm::vec3 &direction)
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

	model_transform->position = bumper->transform->position;
	model_transform->position.y -= (bumper->shape->getHalfHeight() + bumper->shape->getRadius());

	root_hitbox->set_position(bumper->transform->position);

	// set rotation
	if (bumper->walk_direction.x != 0.f || bumper->walk_direction.z != 0.f) {
		glm::vec2 d = { bumper->walk_direction.x, bumper->walk_direction.z };
		model_transform->rotation = direction_to_quat(glm::normalize(d));
	}

	// select animation
	if (!attacking) {
		if (!bumper->on_ground) {
			current_animation = CA_FALLING;
		} else if (bumper->walk_direction.x != 0.f || bumper->walk_direction.z != 0.f) {
			current_animation = CA_RUN;
		} else {
			current_animation = CA_IDLE;
		}
	}
}
	
void Creature::update_animation(float delta)
{
	// non looping animation is finished
	if (attacking) {
		if (character_animation.controller.time_ratio >= 1.f) {
			character_animation.controller.set_looping(true);
			character_animation.controller.set_speed(1.f);
			current_animation = CA_IDLE;
			attacking = false;
		}
	}
	
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
		const glm::mat4 joint = util::ozz_to_mat4(character_animation.models[skeleton_attachments.eyes]);

		glm::mat4 translation = model_transform->to_matrix() * joint;

		eye_position.x = translation[3][0];
		eye_position.y = translation[3][1];
		eye_position.z = translation[3][2];
	}
	// find hand positions
	if (skeleton_attachments.right_hand >= 0) {
		glm::mat4 joint = util::ozz_to_mat4(character_animation.models[skeleton_attachments.right_hand]);
		glm::vec4 offset = { -0.02f, 0.03f, 0.05f, 1.f };

		//glm::vec4 transform = joint * offset;
		//right_hand_transform = model_transform->to_matrix() * joint;
		right_hand_transform = model_transform->to_matrix() * joint;
	}
}
	
void Creature::update_hitboxes()
{
	for (auto &hitbox : hitboxes) {
		const glm::mat4 joint_a = util::ozz_to_mat4(character_animation.models[hitbox.joint_target_a]);
		glm::mat4 translation_a = model_transform->to_matrix() * joint_a;
		hitbox.capsule.a = { translation_a[3][0], translation_a[3][1], translation_a[3][2] };

		const glm::mat4 joint_b = util::ozz_to_mat4(character_animation.models[hitbox.joint_target_b]);
		glm::mat4 translation_b = model_transform->to_matrix() * joint_b;
		hitbox.capsule.b = { translation_b[3][0], translation_b[3][1], translation_b[3][2] };
	}
}
	
void Creature::update_collision(const btDynamicsWorld *world, float delta)
{
	bumper->update(world, delta);
}

void Creature::set_scale(float scale)
{
	unit_scale = scale;

	transform->scale.x = scale;
	transform->scale.y = scale;
	transform->scale.z = scale;

	model_transform->scale.x = FBX_SCALING_FACTOR * scale;
	model_transform->scale.y = FBX_SCALING_FACTOR * scale;
	model_transform->scale.z = FBX_SCALING_FACTOR * scale;

	bumper->set_scale(scale);

	root_hitbox->set_scale(scale);
}

void Creature::attack_request()
{
	if (!attacking) {
		attacking = true;
		current_animation = CA_ATTACK_SLASH;

		character_animation.controller.set_looping(false);
		character_animation.controller.set_time_ratio(0.f);
		character_animation.controller.set_speed(1.5f);
	}
}
