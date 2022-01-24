#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>

#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"
#include "../physics/physical.h"
#include "../physics/trigger.h"
#include "../graphics/mesh.h"
#include "../graphics/model.h"
#include "../graphics/label.h"

#include "entity.h"
#include "settlement.h"
	
Settlement::Settlement()
{
	geom::Sphere sphere = {
		transform.position,
		1.5f
	};

	trigger = std::make_unique<fysx::TriggerSphere>(sphere);
	trigger->ghost_object()->setUserPointer(this);
}

void Settlement::set_position(const glm::vec3 &position)
{
	transform.position = position;
	trigger->set_position(position);
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
