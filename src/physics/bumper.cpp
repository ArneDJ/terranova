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
	ClosestNotMe(btCollisionObject *body) : btCollisionWorld::ClosestRayResultCallback(btVector3(0.0, 0.0, 0.0), btVector3(0.0, 0.0, 0.0))
	{
		me = body;
	}
	virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult &rayResult, bool normalInWorldSpace)
	{
		if (rayResult.m_collisionObject == me) { return 1.0; }

		return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
	}
protected:
	btCollisionObject *me;
};

Bumper::Bumper(const glm::vec3 &origin, float radius, float length)
{
	probe_ground = length;
	probe_air = 0.55f * length;

	float height = length - (2.f * radius);
	if (height < 0.f) {
		height = 0.f;
	}
	
	offset = (0.5f * length) + 0.05f;

	shape = std::make_unique<btCapsuleShape>(radius, height);

	btTransform t;
	t.setIdentity();
	t.setOrigin(vec3_to_bt(origin));

	ghost_object = std::make_unique<btPairCachingGhostObject>();
	ghost_object->setWorldTransform(t);
	ghost_object->setCollisionShape(shape.get());
	ghost_object->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

	transform = std::make_unique<geom::Transform>();
	transform->position = origin;
}
	
void Bumper::update(const btDynamicsWorld *world, float delta)
{
	glm::vec3 pos = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	ghost_object->getWorldTransform().getOrigin().setX(pos.x + (speed * delta * walk_direction.x));
	ghost_object->getWorldTransform().getOrigin().setZ(pos.z + (speed * delta * walk_direction.z));

	ClosestNotMe ray_callback(ghost_object.get());

	// to avoid "snapping" the probe scales if the bumper is on the ground or in the air
	float probe_scale = grounded ? probe_ground : probe_air;
	world->rayTest(ghost_object->getWorldTransform().getOrigin(), ghost_object->getWorldTransform().getOrigin() - btVector3(0.0f, probe_scale, 0.0f), ray_callback);
	if (ray_callback.hasHit()) {
		grounded = true;
		float fraction = ray_callback.m_closestHitFraction;
		glm::vec3 current_pos = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
		float hit_pos = current_pos.y - fraction * probe_scale;

		glm::vec3 hitnormal = bt_to_vec3(ray_callback.m_hitNormalWorld);

		ghost_object->getWorldTransform().getOrigin().setX(current_pos.x);
		ghost_object->getWorldTransform().getOrigin().setY(hit_pos + offset);
		ghost_object->getWorldTransform().getOrigin().setZ(current_pos.z);
	} else {
		grounded = false;
	}

	if (grounded == false) {
		float vertical_velocity = -9.81f * delta;
		float y = ghost_object->getWorldTransform().getOrigin().getY();
		ghost_object->getWorldTransform().getOrigin().setY(y + vertical_velocity);
	}
}
	
void Bumper::sync_transform()
{
	btTransform t = ghost_object->getWorldTransform();

	transform->position = fysx::bt_to_vec3(t.getOrigin());
}
	
void Bumper::teleport(const glm::vec3 &position)
{
	btTransform t;
	t.setIdentity ();
	t.setOrigin(fysx::vec3_to_bt(position));

	ghost_object->setWorldTransform(t);
}

};
