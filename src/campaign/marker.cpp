#include <iostream>
#include <chrono>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"

#include "marker.h"
	
Marker::Marker()
{
	m_transform = std::make_unique<geom::Transform>();
}

void Marker::teleport(const glm::vec3 &position)
{
	m_transform->position = position;
	m_visible = true;
}
	
bool Marker::visible() const
{
	return m_visible;
}
	
const geom::Transform* Marker::transform() const
{
	return m_transform.get();
}
