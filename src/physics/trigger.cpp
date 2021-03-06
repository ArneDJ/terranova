#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/transform.h"

#include "physical.h"
#include "trigger.h"

namespace fysx {

TriggerSphere::TriggerSphere(const geom::Sphere &form)
{
	m_shape = std::make_unique<btSphereShape>(form.radius);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(vec3_to_bt(form.center));

	m_ghost_object = std::make_unique<btGhostObject>();
	m_ghost_object->setCollisionShape(m_shape.get());
	m_ghost_object->setWorldTransform(transform);

	m_transform = std::make_unique<geom::Transform>();
	m_transform->position = form.center;
}

void TriggerSphere::set_position(const glm::vec3 &position) 
{
	m_ghost_object->getWorldTransform().setOrigin(vec3_to_bt(position));
	m_transform->position = position;
}
	
/*
geom::Sphere TriggerSphere::form() const
{
	geom::Sphere sphere = {
		glm::vec3(0.f),
		m_shape->getRadius()
	};

	return sphere;
}
*/
float TriggerSphere::radius() const
{
	return m_shape->getRadius();
}
	
glm::vec3 TriggerSphere::position() const
{
	return bt_to_vec3(m_ghost_object->getWorldTransform().getOrigin());
}

};
