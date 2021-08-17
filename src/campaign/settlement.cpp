#include <iostream>
#include <chrono>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"

#include "settlement.h"
	
Settlement::Settlement()
{
	m_transform = std::make_unique<geom::Transform>();
}

void Settlement::set_position(const glm::vec3 &position)
{
	m_transform->position = position;
}
	
const geom::Transform* Settlement::transform() const
{
	return m_transform.get();
}
