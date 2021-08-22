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
	
PhysicalSystem::~PhysicalSystem()
{
	clear_objects();
}
	
void PhysicalSystem::update(float delta)
{
	m_world->stepSimulation(delta, MAX_SUB_STEPS);
}
	
void PhysicalSystem::update_collision_only()
{
	m_world->performDiscreteCollisionDetection();
}

void PhysicalSystem::add_body(btRigidBody *body, int group, int mask)
{
	m_world->addRigidBody(body, group, mask);
}

void PhysicalSystem::remove_body(btRigidBody *body)
{
	m_world->removeRigidBody(body);
}

void PhysicalSystem::add_object(btCollisionObject *object, int group, int mask)
{
	m_world->addCollisionObject(object, group, mask);
}

void PhysicalSystem::remove_object(btCollisionObject *object)
{
	m_world->removeCollisionObject(object);
}

void PhysicalSystem::clear_objects()
{
	for (int i = m_world->getNumCollisionObjects() - 1; i >= 0; i--) {
		btCollisionObject* obj = m_world->getCollisionObjectArray()[i];
		m_world->removeCollisionObject(obj);
	}
	// just to be sure
	m_world->getCollisionObjectArray().clear();
}
	
RayResult PhysicalSystem::cast_ray(const glm::vec3 &origin, const glm::vec3 &end, int mask) const
{
	RayResult result = { false, glm::vec3(0.f) };

	const btVector3 from = vec3_to_bt(origin);
	const btVector3 to = vec3_to_bt(end);

	btCollisionWorld::ClosestRayResultCallback callback(from, to);
	//callback.m_collisionFilterGroup = mask;
	//callback.m_collisionFilterMask = mask;

	m_world->rayTest(from, to, callback);

	if (callback.hasHit()) {
		result.hit = true;
		result.point = bt_to_vec3(callback.m_hitPointWorld);
	}

	return result;
}

const btDynamicsWorld* PhysicalSystem::world() const
{
	return m_world.get();
}

btDynamicsWorld* PhysicalSystem::world()
{
	return m_world.get();
}

};
