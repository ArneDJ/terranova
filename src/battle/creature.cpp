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

Creature::Creature()
{
	bumper = std::make_unique<fysx::Bumper>(glm::vec3(0.f), 0.25f, 2.f);

	transform = std::make_unique<geom::Transform>();
	
	joint_matrices.buffer.set_target(GL_SHADER_STORAGE_BUFFER);
}
	
void Creature::set_animation(const ozz::animation::Skeleton *skeleton, const ozz::animation::Animation *animation)
{
	character_animation.locals.resize(skeleton->num_soa_joints());
	character_animation.models.resize(skeleton->num_joints());
	character_animation.cache.Resize(animation->num_tracks());

	joint_matrices.data.resize(skeleton->num_joints());
	joint_matrices.update_present();
}
	
void Creature::update(const glm::vec3 &direction, bool jump_request)
{
	/*
	if (jump_request && char_con->onGround()) {
		char_con->jump(btVector3(0,6,0));
	}
	*/
	if (direction != glm::vec3(0,0,0)) {
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
}
	
void Creature::update_animation(const ozz::animation::Skeleton *skeleton, const ozz::animation::Animation *animation, float delta)
{
	if (update_character_animation(&character_animation, animation, skeleton, delta)) {
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
}

