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
	m_trigger->ghost_object()->setUserPointer(this);
}
	
void Settlement::sync()
{
	m_trigger->set_position(m_transform->position);
}
	
const fysx::TriggerSphere* Settlement::trigger() const { return m_trigger.get(); }
	
void Settlement::set_position(const glm::vec3 &position)
{
	m_transform->position = position;
	m_trigger->set_position(position);
}
	
void Settlement::set_faction(uint32_t faction)
{
	m_faction = faction;
}

void Settlement::set_home_tile(uint32_t tile)
{
	m_home_tile = tile;
}
	
void Settlement::add_tile(uint32_t tile)
{
	m_tiles.push_back(tile);
}

const geom::Transform* Settlement::transform() const
{
	return m_transform.get();
}
	
uint32_t Settlement::faction() const
{
	return m_faction;
}

uint32_t Settlement::home_tile() const
{
	return m_home_tile;
}
	
const std::vector<uint32_t>& Settlement::tiles() const
{
	return m_tiles;
}
