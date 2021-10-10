#include <iostream>
#include <vector>
#include <chrono>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../physics/physical.h"
#include "../physics/trigger.h"

#include "settlement.h"
	
Settlement::Settlement()
{
	m_transform = std::make_unique<geom::Transform>();

	geom::Sphere sphere = {
		m_transform->position,
		1.5f
	};
	m_trigger = std::make_unique<fysx::TriggerSphere>(sphere);
}
	
void Settlement::sync()
{
	m_trigger->set_position(m_transform->position);
}
	
void Settlement::expand_radius()
{
	m_tile_radius++;
}

const fysx::TriggerSphere* Settlement::trigger() const { return m_trigger.get(); }
	
uint32_t Settlement::radius() const { return m_tile_radius; }

void Settlement::set_position(const glm::vec3 &position)
{
	m_transform->position = position;
	m_trigger->set_position(position);
}
	
void Settlement::set_faction(uint32_t faction)
{
	m_faction = faction;
}

const geom::Transform* Settlement::transform() const
{
	return m_transform.get();
}
	
uint32_t Settlement::faction() const
{
	return m_faction;
}
