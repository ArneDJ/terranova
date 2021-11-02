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

class BumperConvexCallback : public btCollisionWorld::ClosestConvexResultCallback
{
public:
	BumperConvexCallback(btCollisionObject* me, const btVector3& up, btScalar minSlopeDot)
		: btCollisionWorld::ClosestConvexResultCallback(btVector3(0.0, 0.0, 0.0), btVector3(0.0, 0.0, 0.0)), m_me(me), m_up(up), m_minSlopeDot(minSlopeDot)
	{
	}

	virtual btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace)
	{
		if (convexResult.m_hitCollisionObject == m_me)
			return btScalar(1.0);

		if (!convexResult.m_hitCollisionObject->hasContactResponse())
			return btScalar(1.0);

		btVector3 hitNormalWorld;
		if (normalInWorldSpace) {
			hitNormalWorld = convexResult.m_hitNormalLocal;
		} else {
			///need to transform normal into worldspace
			hitNormalWorld = convexResult.m_hitCollisionObject->getWorldTransform().getBasis() * convexResult.m_hitNormalLocal;
		}

		hit_normals.push_back(bt_to_vec3(hitNormalWorld));

		btScalar dotUp = m_up.dot(hitNormalWorld);
		if (dotUp < m_minSlopeDot)
		{
			return btScalar(1.0);
		}

		return ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
	}

public:
	std::vector<glm::vec3> hit_normals;
protected:
	btCollisionObject* m_me;
	const btVector3 m_up;
	btScalar m_minSlopeDot;
};

glm::vec3 hit_to_velocity(const glm::vec3 &velocity, const glm::vec3 &normal)
{
	glm::vec3 velocity_normalized = glm::normalize(velocity);
	glm::vec3 undesired_motion = normal * glm::dot(velocity_normalized, normal);
	glm::vec3 desired_motion = velocity_normalized - undesired_motion;

	return desired_motion * glm::length(velocity);
}

Bumper::Bumper(const glm::vec3 &origin, float radius, float length)
{
	float height = length - (2.f * radius);
	if (height < 0.f) {
		height = 0.f;
	}
	
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
	
void Bumper::update(btDynamicsWorld *world, float delta)
{
	if (walk_direction == glm::vec3(0.f)) {
		//return;
	}

	// teleport body to desired location
	// find collision hit points, hit normals and distance at location
	// step back to location before first hit 
	// add manifolds to collide and slide vector
	// move body to final location

	glm::vec3 velocity = delta * speed * walk_direction;

	glm::vec3 altered_velocity = collide_and_slide(world, velocity);
	for (int i = 0; i < 2; i++) {
		altered_velocity = collide_and_slide(world, altered_velocity);
	}

	glm::vec3 impulse = local_push(world, delta, speed * delta, velocity);

	glm::vec3 displacement = collide_and_block(world, altered_velocity);
	
	glm::vec3 current_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());

	// lerp
	displacement = glm::mix(displacement, displacement + impulse, 0.1f);

	teleport(current_position + displacement);

	//stick_to_floor(world);
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
	
glm::vec3 Bumper::collide_and_slide(const btDynamicsWorld *world, const glm::vec3 &velocity)
{
	glm::vec3 current_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	glm::vec3 next_position = current_position + velocity;

	glm::vec3 sweep_direction = current_position - next_position;

	btTransform start = ghost_object->getWorldTransform();
	btTransform end = start;
	end.setOrigin(vec3_to_bt(next_position));

	BumperConvexCallback callback(ghost_object.get(), vec3_to_bt(sweep_direction), btScalar(0.0));
	callback.m_collisionFilterGroup = ghost_object->getBroadphaseHandle()->m_collisionFilterGroup;
	callback.m_collisionFilterMask = ghost_object->getBroadphaseHandle()->m_collisionFilterMask;

	ghost_object->convexSweepTest(shape.get(), start, end, callback, world->getDispatchInfo().m_allowedCcdPenetration);

	glm::vec3 updated_velocity = velocity;

	if (callback.hasHit() && ghost_object->hasContactResponse()) {
		//printf("%d\n", callback.hit_normals.size());
		//glm::vec3 hit_normal = bt_to_vec3(callback.m_hitNormalWorld);
		glm::vec3 hit_normal = bt_to_vec3(callback.m_hitNormalWorld);
		//printf("%f\n", fraction);
		glm::vec3 velocity_normalized = glm::normalize(velocity);
		glm::vec3 undesired_motion = hit_normal * glm::dot(velocity_normalized, hit_normal);
		glm::vec3 desired_motion = velocity_normalized - undesired_motion;
		updated_velocity = desired_motion * glm::length(velocity);
		//}
		// Remove penetration (penetration epsilon added to handle infinitely small penetration):
		//float fraction = callback.m_closestHitFraction;
		//float depth = (1.f - fraction);
		//printf("%f\n", depth);
		//current_position += (hit_normal * (depth + 0.001f));
	}
		
	//teleport(current_position + updated_velocity);
	return updated_velocity;
}
	
glm::vec3 Bumper::local_push(btDynamicsWorld *world, float delta, float speed, const glm::vec3 &velocity)
{
	// TODO check what this does
	//btVector3 minAabb, maxAabb;
	//shape->getAabb(ghost_object->getWorldTransform(), minAabb, maxAabb);
	//world->getBroadphase()->setAabb(ghost_object->getBroadphaseHandle(), minAabb, maxAabb, world->getDispatcher());

	world->getDispatcher()->dispatchAllCollisionPairs(ghost_object->getOverlappingPairCache(), world->getDispatchInfo(), world->getDispatcher());

	glm::vec3 current_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	glm::vec3 displacement = glm::vec3(0.f);

	for (int i = 0; i < ghost_object->getOverlappingPairCache()->getNumOverlappingPairs(); i++) {
		btBroadphasePair *pair = &ghost_object->getOverlappingPairCache()->getOverlappingPairArray()[i];

		btCollisionObject *obj0 = static_cast<btCollisionObject*>(pair->m_pProxy0->m_clientObject);
		btCollisionObject *obj1 = static_cast<btCollisionObject*>(pair->m_pProxy1->m_clientObject);

		if ((obj0 && !obj0->hasContactResponse()) || (obj1 && !obj1->hasContactResponse())) {
			continue;
		}

		btManifoldArray manifolds;
		if (pair->m_algorithm) {
			pair->m_algorithm->getAllContactManifolds(manifolds);
		}
		for (int j = 0; j < manifolds.size(); j++) {
			btPersistentManifold* manifold = manifolds[j];
			float sign = manifold->getBody0() == ghost_object.get() ? -1.f : 1.f;

			for (int p = 0; p < manifold->getNumContacts(); p++) {
				const btManifoldPoint &point = manifold->getContactPoint(p);

				// Remove penetration (penetration epsilon added to handle infinitely small penetration):
				float dist = point.getDistance();
				//printf("%f\n", dist);

				glm::vec3 impulse = bt_to_vec3(point.m_normalWorldOnB) * (sign * dist + 0.001f);
				displacement += impulse;
			}
		}
	}


	/*
	if (displacement != glm::vec3(0.f)) {
		current_position = glm::mix(current_position, current_position + displacement, 0.5f * glm::length(glm::normalize(displacement)));
		teleport(current_position);
	}
	*/
	return displacement;
}

glm::vec3 Bumper::collide_and_block(const btDynamicsWorld *world, const glm::vec3 &velocity)
{
	glm::vec3 displacement = velocity;

	glm::vec3 current_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	glm::vec3 next_position = current_position + velocity;

	glm::vec3 sweep_direction = current_position - next_position;

	btTransform start = ghost_object->getWorldTransform();
	btTransform end = start;
	end.setOrigin(vec3_to_bt(next_position));

	BumperConvexCallback callback(ghost_object.get(), vec3_to_bt(sweep_direction), btScalar(0.0));
	callback.m_collisionFilterGroup = ghost_object->getBroadphaseHandle()->m_collisionFilterGroup;
	callback.m_collisionFilterMask = ghost_object->getBroadphaseHandle()->m_collisionFilterMask;

	ghost_object->convexSweepTest(shape.get(), start, end, callback, world->getDispatchInfo().m_allowedCcdPenetration);

	glm::vec3 dir = glm::normalize(velocity);
	float distance = glm::length(velocity);

	/*
	glm::vec3 hit_normal = bt_to_vec3(callback.m_hitNormalWorld);
	//printf("%f\n", fraction);
	glm::vec3 velocity_normalized = glm::normalize(velocity);
	glm::vec3 undesired_motion = hit_normal * glm::dot(velocity_normalized, hit_normal);
	glm::vec3 desired_motion = velocity_normalized - undesired_motion;
	updated_velocity = desired_motion * glm::length(velocity);
	*/

	if (callback.hasHit() && ghost_object->hasContactResponse()) {
		glm::vec3 hit_normal = bt_to_vec3(callback.m_hitNormalWorld);
		float fraction = callback.m_closestHitFraction;
		float depth = fraction * glm::length(next_position - current_position);
		//printf("distance %f\n", distance);
		//printf("fraction %f\n", fraction);
		//printf("depth %f\n", depth);
		if (distance > depth) {
			displacement = depth * dir;
		}
	}

	return displacement;
}
	
void Bumper::stick_to_floor(const btDynamicsWorld *world)
{
	ClosestNotMe ray_callback(ghost_object.get());

	world->rayTest(ghost_object->getWorldTransform().getOrigin(), ghost_object->getWorldTransform().getOrigin() - btVector3(0.0f, 2.f, 0.0f), ray_callback);

	if (ray_callback.hasHit()) {
		grounded = true;
		float fraction = ray_callback.m_closestHitFraction;
		glm::vec3 current_pos = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
		float hit_pos = current_pos.y - fraction * 2.f;

		current_pos.y = hit_pos + 1.f;
	
		teleport(current_pos);

	} else {
		grounded = false;
	}
}

};

