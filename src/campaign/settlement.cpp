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

uint32_t Town::county() const
{
	return m_county;
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

void Town::set_county(uint32_t county)
{
	m_county = county;
}

void Town::set_position(const glm::vec3 &position)
{
	m_transform->position = position;
	m_trigger->set_position(position);
}

uint32_t County::id() const
{
	return m_id;
}

uint32_t County::faction() const
{
	return m_faction;
}

uint32_t County::town() const
{
	return m_town;
}

const std::vector<uint32_t>& County::tiles() const
{
	return m_tiles;
}

void County::set_id(uint32_t id)
{
	m_id = id;
}

void County::set_faction(uint32_t faction)
{
	m_faction = faction;
}

void County::set_town(uint32_t town)
{
	m_town = town;
}

void County::add_tile(uint32_t tile)
{
	m_tiles.push_back(tile);
}
