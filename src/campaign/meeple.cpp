#include <list>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../util/animation.h"
#include "../physics/physical.h"
#include "../physics/trigger.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/label.h"

#include "entity.h"
#include "meeple.h"

static const float MEEPLE_VISIBILITY_RADIUS = 50.F;

void PathFinder::set_nodes(const std::list<glm::vec2> &nodes)
{
	m_nodes.clear();

	// need at least 2 nodes to create a valid path
	if (nodes.size() > 1) {
		m_nodes.assign(nodes.begin(), nodes.end());
		m_state = PathState::NEXT_NODE;
	} else {
		m_state = PathState::FINISHED;
	}
}

void PathFinder::update(float delta, float speed)
{
	if (m_state == PathState::NEXT_NODE) {
		std::list<glm::vec2>::iterator itr0 = m_nodes.begin();
		std::list<glm::vec2>::iterator itr1 = std::next(m_nodes.begin());
		m_origin = *itr0;
		m_destination = *itr1;
		m_velocity = glm::normalize(m_destination - m_origin);
		m_radius = glm::distance(m_origin, m_destination);
		m_state = PathState::MOVING;
	} else if (m_state == PathState::MOVING) {
		glm::vec2 offset = m_location + (delta * speed * m_velocity);
		if (geom::point_in_circle(offset, m_origin, m_radius)) {
			m_location = offset;
		} else {
			// arrived at node
			m_location = m_destination;
			m_nodes.pop_front(); 
			m_state = PathState::NEXT_NODE;
		}
	}

	if (m_nodes.size() < 2) {
		m_state = PathState::FINISHED;
	}
}

void PathFinder::teleport(const glm::vec2 &position)
{
	clear_path();
	m_origin = position;
	m_location = position;
}

void PathFinder::clear_path()
{
	m_nodes.clear();
	m_state = PathState::FINISHED;
}
	
glm::vec2 PathFinder::location() const { return m_location; }

glm::vec2 PathFinder::destination() const { return m_destination; }

glm::vec2 PathFinder::velocity() const { return m_velocity; }
	
PathState PathFinder::state() const { return m_state; }

Meeple::Meeple()
{
	geom::Sphere sphere = {
		transform.position,
		1.5f
	};
	m_trigger = std::make_unique<fysx::TriggerSphere>(sphere);

	sphere.radius = MEEPLE_VISIBILITY_RADIUS;
	m_visibility = std::make_unique<fysx::TriggerSphere>(sphere);

	m_visibility->ghost_object()->setUserPointer(this);
	m_trigger->ghost_object()->setUserPointer(this);

	transform.scale = glm::vec3(0.005f);
	
	m_joint_matrices.buffer.set_target(GL_SHADER_STORAGE_BUFFER);
}

const fysx::TriggerSphere* Meeple::trigger() const { return m_trigger.get(); }

const fysx::TriggerSphere* Meeple::visibility() const { return m_visibility.get(); }

/*
glm::vec2 Meeple::map_position() const
{
	return glm::vec2(transform.position.x, transform.position.z);
}
*/
	
PathState Meeple::path_state() const
{
	return m_path_finder.state();
}

void Meeple::set_speed(float speed) { m_speed = speed; }
	
void Meeple::set_path(const std::list<glm::vec2> &nodes)
{
	m_path_finder.set_nodes(nodes);
}

void Meeple::set_vertical_offset(float offset)
{
	transform.position.y = offset;

	glm::vec3 trigger_position = transform.position;
	trigger_position.y += 1.f;
	m_trigger->set_position(trigger_position);
	m_visibility->set_position(trigger_position);
}

void Meeple::update(float delta)
{
	auto prev_path_state = m_path_finder.state();

	m_path_finder.update(delta, m_speed);

	glm::vec2 location = m_path_finder.location();
	transform.position.x = location.x;
	transform.position.z = location.y;

	glm::vec3 trigger_position = transform.position;
	trigger_position.y += 1.f;
	m_trigger->set_position(trigger_position);
	m_visibility->set_position(trigger_position);

	// update rotation
	if (m_path_finder.state() == PathState::MOVING) {
		glm::vec2 direction = glm::normalize(m_path_finder.velocity());
		float angle = atan2(direction.x, direction.y);
		glm::mat4 R = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
		transform.rotation = glm::quat(R);
	}
}
	
void Meeple::update_animation(float delta)
{
	if (m_path_finder.state() != PathState::FINISHED) {
		m_animation = MA_RUN;
		moving = true;
	} else {
		m_animation = MA_IDLE;
		moving = false;
	}
	m_animation_controller->update(m_animation, delta);

	for (const auto &skin : model->skins()) {
		if (skin->inverse_binds.size() == m_animation_controller->models.size()) {
			for (int i = 0; i < m_animation_controller->models.size(); i++) {
				m_joint_matrices.data[i] = util::ozz_to_mat4(m_animation_controller->models[i]) * skin->inverse_binds[i];
			}
			break; // only animate first skin
		}
	}
	m_joint_matrices.update_present();
}

void Meeple::teleport(const glm::vec2 &position)
{
	transform.position.x = position.x;
	transform.position.z = position.y;

	m_path_finder.teleport(position);
}

void Meeple::clear_path()
{
	m_path_finder.clear_path();
}
	
void Meeple::clear_target()
{
	target_type = 0;
	target_id = 0;

	clear_path();
}

void Meeple::sync()
{
	m_path_finder.teleport(glm::vec2(transform.position.x, transform.position.z));
}

void Meeple::set_animation(const util::AnimationSet *set)
{
	m_animation_controller = std::make_unique<util::AnimationController>(set);
	m_joint_matrices.data.resize(set->skeleton->num_joints());
	m_joint_matrices.update_present();
}
	
void Meeple::display() const
{
	m_joint_matrices.buffer.bind_base(0);
		
	model->display();
}
	
void MeepleController::update(float delta)
{
	for (auto &mapping : meeples) {
		auto &meeple = mapping.second;
		meeple->update(delta);
		//meeple->update_animation(delta);
	}
}
	
void MeepleController::add_ticks(uint64_t ticks)
{
	for (auto &mapping : meeples) {
		auto &meeple = mapping.second;
		meeple->ticks += ticks;
	}
}
	
void MeepleController::clear()
{
	meeples.clear();
}
	
/*
void MeepleController::check_visibility()
{
	for (auto &mapping : meeples) {
		auto &meeple = mapping.second;
		meeple->visible = false;
	}
	const auto &visibility = player->visibility()->ghost_object();
	int count = visibility->getNumOverlappingObjects();
	for (int i = 0; i < count; i++) {
		btCollisionObject *obj = visibility->getOverlappingObject(i);
		CampaignEntity entity_type = CampaignEntity(obj->getUserIndex2());
		if (entity_type == CampaignEntity::MEEPLE) {
			Meeple *target = static_cast<Meeple*>(obj->getUserPointer());
			if (target) {
				target->visible = true;
			}
		}
	}

	// make player always visible
	player->visible = true;
}
*/
