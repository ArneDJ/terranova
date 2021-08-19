#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/transform.h"

#include "physical.h"
#include "bumper.h"

namespace fysx {
	
class ClosestNotMe : public btCollisionWorld::ClosestRayResultCallback {
public:
	ClosestNotMe(btRigidBody *body) : btCollisionWorld::ClosestRayResultCallback(btVector3(0.0, 0.0, 0.0), btVector3(0.0, 0.0, 0.0))
	{
		me = body;
	}
	virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult &rayResult, bool normalInWorldSpace)
	{
		if (rayResult.m_collisionObject == me) { return 1.0; }

		return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
	}
protected:
	btRigidBody *me;
};

Bumper::Bumper(const glm::vec3 &origin, float radius, float length)
{
	m_height = length;
	m_probe_ground = length;
	m_probe_air = 0.55f * length; // FIXME automate

	m_capsule_radius = radius;

	// The total height is height+2*radius, so the height is just the height between the center of each 'sphere' of the capsule caps
	m_capsule_height = length - (0.25f * length);
	m_capsule_height -= 2.f * radius;
	if (m_capsule_height < 0.f) {
		m_capsule_height = 0.f;
	}

	m_shape = std::make_unique<btCapsuleShape>(radius, m_capsule_height);

	btScalar mass(1.f);

	btVector3 inertia(0, 0, 0);
	m_shape->calculateLocalInertia(mass, inertia);

	m_offset = (0.5f * (m_capsule_height + (2.f * radius))) + (0.25f * length);
	m_body_offset =  m_offset - (0.5f * length);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(vec3_to_bt(origin));

	m_motion = std::make_unique<btDefaultMotionState>(transform);
	btRigidBody::btRigidBodyConstructionInfo rbinfo(mass, m_motion.get(), m_shape.get(), inertia);
	m_body = std::make_unique<btRigidBody>(rbinfo);
	m_body->setSleepingThresholds(0.f, 0.f);
	m_body->setAngularFactor(0.f);
	m_body->setFriction(0.f);
	m_body->setRestitution(0.f);

	m_body->setActivationState(DISABLE_DEACTIVATION);

	m_transform = std::make_unique<geom::Transform>();
	m_transform->position = origin;
}

void Bumper::update(const btDynamicsWorld *world)
{
	ClosestNotMe ray_callback(m_body.get());
	//ray_callback.m_collisionFilterGroup = COLLISION_GROUP_ACTOR;
	//ray_callback.m_collisionFilterMask = COLLISION_GROUP_ACTOR | COLLISION_GROUP_HEIGHTMAP | COLLISION_GROUP_WORLD;

	// to avoid "snapping" the probe scales if the bumper is on the ground or in the air
	float probe_scale = m_grounded ? m_probe_ground : m_probe_air;

	world->rayTest(m_body->getWorldTransform().getOrigin(), m_body->getWorldTransform().getOrigin() - btVector3(0.0f, probe_scale, 0.0f), ray_callback);
	if (ray_callback.hasHit()) {
		m_grounded = true;
		float fraction = ray_callback.m_closestHitFraction;
		glm::vec3 current_pos = bt_to_vec3(m_body->getWorldTransform().getOrigin());
		float hit_pos = current_pos.y - fraction * probe_scale;

		glm::vec3 hitnormal = bt_to_vec3(ray_callback.m_hitNormalWorld);

		m_body->getWorldTransform().getOrigin().setX(current_pos.x);
		m_body->getWorldTransform().getOrigin().setY(hit_pos+m_offset);
		m_body->getWorldTransform().getOrigin().setZ(current_pos.z);

		m_velocity.y = 0.f;
		//float slope = hitnormal.y;
	} else {
		m_grounded = false;
		m_velocity.y = m_body->getLinearVelocity().getY();
	}

	m_body->setLinearVelocity(vec3_to_bt(m_velocity));

	m_velocity.x = 0.f;
	m_velocity.z = 0.f;

	if (m_grounded) {
		m_body->setGravity({ 0, 0, 0 });
	} else {
		m_body->setGravity(world->getGravity());
	}
	
	// update capsule transform
	m_transform->position = bt_to_vec3(m_body->getWorldTransform().getOrigin());
	m_transform->rotation = bt_to_quat(m_body->getWorldTransform().getRotation());
}

void Bumper::jump()
{
	if (m_grounded) {
		m_grounded = false; // valid jump request so we are no longer on ground
		m_body->applyCentralImpulse(btVector3(m_velocity.x, 5.f, m_velocity.z));

		// Move upwards slightly so velocity isn't immediately canceled when it detects it as on ground next frame
		const float offset = 0.01f;

		float previous_Y = m_body->getWorldTransform().getOrigin().getY();

		m_body->getWorldTransform().getOrigin().setY(previous_Y + offset);
	}
}

const geom::Transform* Bumper::transform() const
{
	return m_transform.get();
}

btRigidBody* Bumper::body() const
{
	return m_body.get();
}

glm::vec3 Bumper::standing_position() const
{
	btTransform transform;
	m_motion->getWorldTransform(transform);

	glm::vec3 translation = bt_to_vec3(transform.getOrigin());
	// substract vertical offset
	translation.y -= m_body_offset + (0.5f * m_height);

	return translation;
}

bool Bumper::grounded() const
{
	return m_grounded;
}

void Bumper::set_velocity(float x, float z)
{
	m_velocity.x = x;
	m_velocity.z = z;
}

};
