#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/transform.h"
#include "../geometry/geometry.h"

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

class BumperConvexCallback : public btCollisionWorld::ClosestConvexResultCallback {
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

		/*
		btScalar dotUp = m_up.dot(hitNormalWorld);
		if (dotUp < m_minSlopeDot) {
			return btScalar(1.0);
		}
		*/

		return ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
	}

protected:
	btCollisionObject *m_me;
	const btVector3 m_up = btVector3(0, 1, 0);
	btScalar m_minSlopeDot;
};

struct Trace {
	float fraction = 1.f; // time completed, 1.0 = didn't hit anything
	glm::vec3 endpos; // final position
	glm::vec3 normal; // surface normal at impact
};

static const float V_GRAVITY = 9.81F;
static const float TERMINAL_VELOCITY = 55.F;

Trace trace_movement(btPairCachingGhostObject *object, const btConvexShape *shape, const btDynamicsWorld *world, const glm::vec3 &origin, const glm::vec3 &destination)
{
	Trace trace = {};
	trace.fraction = 1.f;
	trace.endpos = origin;

	glm::vec3 sweep_direction = origin - destination;

	btTransform start = object->getWorldTransform();
	btTransform end = start;
	end.setOrigin(vec3_to_bt(destination));

	BumperConvexCallback callback(object, vec3_to_bt(sweep_direction), 0.45);
	callback.m_collisionFilterGroup = object->getBroadphaseHandle()->m_collisionFilterGroup;
	callback.m_collisionFilterMask = object->getBroadphaseHandle()->m_collisionFilterMask;

	object->convexSweepTest(shape, start, end, callback, world->getDispatchInfo().m_allowedCcdPenetration);

	glm::vec3 direction = destination - origin;

	if (callback.hasHit() && object->hasContactResponse()) {
		glm::vec3 hitpoint = bt_to_vec3(callback.m_hitPointWorld);
		trace.fraction = callback.m_closestHitFraction;
		trace.normal = bt_to_vec3(callback.m_hitNormalWorld);
		trace.endpos = origin + (trace.fraction * direction);
	}

	return trace;
}
	

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
	
void Bumper::update(const btDynamicsWorld *world, float delta)
{
	const glm::vec3 start_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());

	glm::vec3 displacement = speed * delta * walk_direction;
	collide_and_slide(world, displacement);
		
	apply_gravity(world, delta);

	const glm::vec3 end_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());

	update_fallen_distance(start_position, end_position);
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
	
void Bumper::apply_gravity(const btDynamicsWorld *world, float delta)
{
	glm::vec3 gravity = { 0.f, -V_GRAVITY, 0.f };

	if (!on_ground) {
		gravity.y = -sqrtf(6.f * V_GRAVITY * fallen_distance);
		if (fabs(gravity.y) > TERMINAL_VELOCITY) {
			gravity.y = -TERMINAL_VELOCITY;
		}
		// in case fallen distance was 0 (start of fall)
		if (!gravity.y) {
			gravity.y = -V_GRAVITY;
		}
	}

	gravity.y *= delta;
	
	// find closest ground collision
	const glm::vec3 origin = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	glm::vec3 destination = origin + gravity;
	Trace trace = trace_movement(ghost_object.get(), shape.get(), world, origin, destination);

	on_ground = (trace.fraction < 1.f);

	if (on_ground) {
		// stick on ground if not in air
		glm::vec3 moved = glm::normalize(gravity) * (trace.fraction * glm::length(gravity - 0.1f));
		destination = origin + moved;
		on_ground = true;
	}
		
	teleport(destination);
}
	
void Bumper::apply_velocity(const glm::vec3 &velocity)
{
	glm::vec3 current_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	teleport(current_position + velocity);
}

void Bumper::collide_and_slide(const btDynamicsWorld *world, const glm::vec3 &displacement)
{
	// no motion early exit
	if (glm::length(displacement) < 1e-6f) {
		return;
	}

	static const float CLIP_OFFSET = 0.01F;

	const glm::vec3 origin = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());

	glm::vec3 position = origin;
	glm::vec3 start = origin;
	glm::vec3 end = origin + displacement;
	glm::vec3 motion = displacement;

	for (int i = 0; i < 4; i++) {
		if (glm::length(motion) < 1e-6f) {
			return;
		}
		// find collision with convex sweep
		Trace trace = trace_movement(ghost_object.get(), shape.get(), world, start, end);
		// hit nothing early exit
		if (trace.fraction >= 1.f) {
			position = end;
			break;
		}
		// we must have hit something
		// find the position right before the collision occurs
		glm::vec3 moved = glm::normalize(motion) * (trace.fraction * glm::length(motion) - CLIP_OFFSET);
		start += moved;

		// update last visited position
		position = start;

		// remaining motion
		glm::vec3 remainder = end - start;
		// slide remaining motion along hit normal
		glm::vec3 direction = glm::normalize(remainder);
		float d = glm::dot(direction, trace.normal);
		glm::vec3 slide = direction - (d * trace.normal);

		motion = glm::length(remainder) * slide;

		end = start + motion;
	}

	motion = position - origin;
	// to avoid occilations somewhat lerp with original position
	if (glm::dot(glm::normalize(motion), glm::normalize(displacement)) <= 0.f) {
		position = glm::mix(origin, position, 0.25f);
	}

	teleport(position);
}
	
void Bumper::update_fallen_distance(const glm::vec3 &start, const glm::vec3 &end)
{
	if (on_ground) {
		fallen_distance = 0.f;
	} else {
		fallen_distance += glm::distance(start.y, end.y);
	}
}

};

