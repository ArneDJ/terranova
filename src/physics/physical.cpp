#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "physical.h"

namespace fysx {

static const int MAX_SUB_STEPS = 10;
static const btVector3 GRAVITY = { 0.F, -9.81F, 0.F }; // same gravity as in my house

PhysicalSystem::PhysicalSystem()
{
	m_config = std::make_unique<btDefaultCollisionConfiguration>();
	m_dispatcher = std::make_unique<btCollisionDispatcher>(m_config.get());
	m_broadphase = std::make_unique<btDbvtBroadphase>();
	m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();

	m_world = std::make_unique<btDiscreteDynamicsWorld>(m_dispatcher.get(), m_broadphase.get(), m_solver.get(), m_config.get());
	m_world->setGravity(GRAVITY);
}
	
void PhysicalSystem::update(float delta)
{
	m_world->stepSimulation(delta, MAX_SUB_STEPS);
}

void PhysicalSystem::add_body(btRigidBody *body)
{
	m_world->addRigidBody(body);
}

void PhysicalSystem::remove_body(btRigidBody *body)
{
	m_world->removeRigidBody(body);
}
	
void PhysicalSystem::clear_objects()
{
	// remove the rigidbodies from the dynamics world
	for (int i = 0; i < m_world->getNumCollisionObjects(); i++) {
		btCollisionObject *obj = m_world->getCollisionObjectArray()[i];
		m_world->removeCollisionObject(obj);
	}
}

};
