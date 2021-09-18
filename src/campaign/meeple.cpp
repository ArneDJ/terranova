#include <list>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../physics/physical.h"
#include "../physics/trigger.h"

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
	m_nodes.clear();
	m_state = PathState::FINISHED;
	m_origin = position;
	m_location = position;
}

glm::vec2 PathFinder::location() const { return m_location; }

glm::vec2 PathFinder::destination() const { return m_destination; }

glm::vec2 PathFinder::velocity() const { return m_velocity; }
	
PathState PathFinder::state() const { return m_state; }

Meeple::Meeple()
{
	m_transform = std::make_unique<geom::Transform>();

	geom::Sphere sphere = {
		m_transform->position,
		1.5f
	};
	m_trigger = std::make_unique<fysx::TriggerSphere>(sphere);

	sphere.radius = MEEPLE_VISIBILITY_RADIUS;
	m_visibility = std::make_unique<fysx::TriggerSphere>(sphere);

	m_visibility->ghost_object()->setUserPointer(this);
	m_trigger->ghost_object()->setUserPointer(this);
}

const geom::Transform* Meeple::transform() const { return m_transform.get(); }
	
const fysx::TriggerSphere* Meeple::trigger() const { return m_trigger.get(); }

const fysx::TriggerSphere* Meeple::visibility() const { return m_visibility.get(); }

glm::vec2 Meeple::position() const
{
	return glm::vec2(m_transform->position.x, m_transform->position.z);
}

void Meeple::set_speed(float speed) { m_speed = speed; }
	
void Meeple::set_name(const std::string &name) { m_name = name; }

void Meeple::set_path(const std::list<glm::vec2> &nodes)
{
	m_path_finder.set_nodes(nodes);
}

void Meeple::update(float delta)
{
	m_path_finder.update(delta, m_speed);

	glm::vec2 location = m_path_finder.location();
	m_transform->position.x = location.x;
	m_transform->position.z = location.y;

	// FIXME autocalculate this
	glm::vec3 trigger_position = m_transform->position;
	trigger_position.y += 1.f;
	m_trigger->set_position(trigger_position);
	m_visibility->set_position(trigger_position);

	// update rotation
	if (m_path_finder.state() == PathState::MOVING) {
		glm::vec2 direction = glm::normalize(m_path_finder.velocity());
		float angle = atan2(direction.x, direction.y);
		glm::mat4 R = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0.f, 1.f, 0.f));
		m_transform->rotation = glm::quat(R);
	}
}

void Meeple::teleport(const glm::vec2 &position)
{
	m_transform->position.x = position.x;
	m_transform->position.z = position.y;

	m_path_finder.teleport(position);
}

void Meeple::sync()
{
	m_path_finder.teleport(glm::vec2(m_transform->position.x, m_transform->position.z));
}
	
void Meeple::add_troops(uint32_t troop_type, int count)
{
	int instances = 0;

	auto search = m_troops.find(troop_type);
	if (search != m_troops.end()) {
		instances = search->second;
	}

	instances += count;

	if (instances < 0) {
		instances = 0;
	}
		
	m_troops[troop_type] = instances;
}
	
MeepleController::MeepleController()
{
	current_chase = chases.begin();
}

void MeepleController::update(float delta)
{
	// visibility collision triggers
	//puts("visibility collision triggers");
	for (auto &meeple : meeples) {
		if (meeple->state() == MeepleState::ROAMING) {
			auto visibility = meeple->visibility();
			auto ghost_object = visibility->ghost_object();
			int count = ghost_object->getNumOverlappingObjects();
			for (int i = 0; i < count; i++) {
				btCollisionObject *obj = ghost_object->getOverlappingObject(i);
				Meeple *target = static_cast<Meeple*>(obj->getUserPointer());
				if (target) {
					if (target != meeple.get()) {
						float dist = glm::distance(meeple->position(), target->position());
						if (dist < MEEPLE_VISIBILITY_RADIUS) {
				
							meeple->set_state(MeepleState::TRACKING);
							add_chase(meeple.get(), target);
							break;
						}
					}
				}
			}
		}
	}

	// update chases
	chase_time_slice += delta;
	if (chases.size() > 0 && chase_time_slice > 0.02f) {
		chase_time_slice = 0.f;
		std::advance(current_chase, 1);
	}
	if (current_chase == chases.end()) {
		current_chase = chases.begin();
	}

	player->update(delta);
	for (auto &meeple : meeples) {
		meeple->update(delta);
	}
}
	
void MeepleController::add_chase(Meeple *chaser, Meeple *target)
{
	for (const auto &chase : chases) {
		if (chase->chaser == chaser && chase->target == target) {
			return;
		}
	}

	auto chase = std::make_unique<MeepleChase>();
	chase->chaser = chaser;
	chase->target = target;
	chase->finished = false;

	chases.push_back(std::move(chase));
}
	
void MeepleController::clear()
{
	chases.clear();
	current_chase = chases.begin();
	meeples.clear();
}