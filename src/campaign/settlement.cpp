#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../physics/physical.h"
#include "../physics/trigger.h"

#include "settlement.h"
	
Town::Town()
{
	m_transform = std::make_unique<geom::Transform>();

	geom::Sphere sphere = {
		m_transform->position,
		1.5f
	};

	m_trigger = std::make_unique<fysx::TriggerSphere>(sphere);
	m_trigger->ghost_object()->setUserPointer(this);
}

uint32_t Town::id() const
{
	return m_id;
}

uint32_t Town::faction() const
{
	return m_faction;
}

uint32_t Town::tile() const
{
	return m_tile;
}

uint32_t Town::fiefdom() const
{
	return m_fiefdom;
}
	
uint8_t Town::size() const
{
	return m_size;
}

glm::vec2 Town::map_position() const
{
	return glm::vec2(m_transform->position.x, m_transform->position.z);
}

const fysx::TriggerSphere* Town::trigger() const { return m_trigger.get(); }

const geom::Transform* Town::transform() const
{
	return m_transform.get();
}

void Town::set_id(uint32_t id)
{
	m_id = id;
	m_trigger->ghost_object()->setUserIndex(id);
}

void Town::set_faction(uint32_t faction)
{
	m_faction = faction;
}

void Town::set_tile(uint32_t tile)
{
	m_tile = tile;
}

void Town::set_fiefdom(uint32_t fiefdom)
{
	m_fiefdom = fiefdom;
}
	
void Town::set_size(uint8_t size)
{
	m_size = size;
}

void Town::set_position(const glm::vec3 &position)
{
	m_transform->position = position;
	m_trigger->set_position(position);
}

uint32_t Fiefdom::id() const
{
	return m_id;
}

uint32_t Fiefdom::faction() const
{
	return m_faction;
}

uint32_t Fiefdom::town() const
{
	return m_town;
}

const std::vector<uint32_t>& Fiefdom::tiles() const
{
	return m_tiles;
}

void Fiefdom::set_id(uint32_t id)
{
	m_id = id;
}

void Fiefdom::set_faction(uint32_t faction)
{
	m_faction = faction;
}

void Fiefdom::set_town(uint32_t town)
{
	m_town = town;
}

void Fiefdom::add_tile(uint32_t tile)
{
	m_tiles.push_back(tile);
}
