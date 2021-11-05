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

struct Trace {
	float fraction = 1.f; // time completed, 1.0 = didn't hit anything
	glm::vec3 endpos; // final position
	glm::vec3 normal; // surface normal at impact
};
	
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

		btScalar dotUp = m_up.dot(hitNormalWorld);
		if (dotUp < m_minSlopeDot)
		{
			return btScalar(1.0);
		}

		return ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
	}

protected:
	btCollisionObject* m_me;
	const btVector3 m_up;
	btScalar m_minSlopeDot;
};

Trace trace_movement(btPairCachingGhostObject *object, const btConvexShape *shape, const btDynamicsWorld *world, const glm::vec3 &origin, const glm::vec3 &destination);

#define	STOP_EPSILON 0.1f

int clip_velocity(const glm::vec3 &in, const glm::vec3 &normal, glm::vec3 &out, float overbounce)
{

	int blocked = 0;

	if (normal.y > 0) {
		blocked |= 1;		// floor
	}
	if (!normal.y) {
		blocked |= 2;		// step
	}

	float backoff = glm::dot(in, normal) * overbounce;

	for (int i = 0; i < 3; i++) {
		float change = normal[i] * backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON) {
			out[i] = 0;
		}
	}

	return blocked;
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
	
void Bumper::update(btDynamicsWorld *world, float delta)
{
	glm::vec3 velocity = speed * walk_direction;

	//try_move(world, velocity, delta);
	velocity = collide_and_slide(world, walk_direction, speed * delta);

	teleport(velocity);
	//apply_velocity(velocity);
		/*
	glm::vec3 impulse = local_push(world, delta, speed);


	//printf("%f, %f, %f\n", impulse.x, impulse.y, impulse.z);

	//impulse = collide_and_slide(world, impulse);
	//impulse = collide_and_block(world, impulse);

	glm::vec3 slide_velocity = collide_and_slide(world, velocity);

	//glm::vec3 blocked_velocity = collide_and_block(world, slide_velocity);

	//velocity = glm::mix(blocked_velocity, blocked_velocity + impulse, 0.5f);
	
	apply_velocity(slide_velocity);
	*/
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
	
glm::vec3 Bumper::collide_and_slide(const btDynamicsWorld *world, const glm::vec3 &velocity, float delta)
{
	glm::vec3 origin = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());

	float time_left = delta;

	glm::vec3 new_velocity = time_left * velocity;

	glm::vec3 final_position = origin;
		
	glm::vec3 position = origin;
	glm::vec3 end = origin + new_velocity;

	for (int i = 0; i < 4; i++) {
		// collide
		Trace trace = trace_movement(ghost_object.get(), shape.get(), world, position, end);

		// hit nothing
		if (trace.fraction >= 1.f) {
			final_position = end;
			break;
		} else {
			glm::vec3 endpos = position + 0.0001f * (trace.endpos - position);
			//glm::vec3 endpos = trace.endpos;

			position = endpos;
			final_position = endpos;
			// slide
			glm::vec3 direction = glm::normalize(new_velocity);
			float d = glm::dot(direction, trace.normal);
			glm::vec3 nv = direction - (d * trace.normal);

			//glm::vec3 new_position = origin + delta * glm::length(velocity) * nv;
			glm::vec3 new_position = endpos + time_left * nv;

			new_velocity = new_position - endpos;

			end = new_position;


			time_left -= trace.fraction * delta;
		}
	}

	glm::vec3 final_velocity = final_position - origin;
	// if velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
	if (glm::dot(final_velocity, velocity) <= 0.f) {
		final_velocity = glm::vec3(0.f);
	}

	return origin + final_velocity;
}
	
// TODO this is wrong, find a more accurate push
// finds penetration points and adds them to a vector to push away from them
glm::vec3 Bumper::local_push(btDynamicsWorld *world, float delta, float speed)
{
	// TODO check what this does
	btVector3 minAabb, maxAabb;
	shape->getAabb(ghost_object->getWorldTransform(), minAabb, maxAabb);
	world->getBroadphase()->setAabb(ghost_object->getBroadphaseHandle(), minAabb, maxAabb, world->getDispatcher());

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

				// distance between what?
				float dist = point.getDistance();
				//printf("%f\n", dist);

				glm::vec3 impulse = bt_to_vec3(point.m_normalWorldOnB) * (sign * dist);
				displacement += impulse;
			}
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
	
void Bumper::apply_velocity(const glm::vec3 &velocity)
{
	glm::vec3 current_position = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());
	teleport(current_position + velocity);
}

#define	MAX_CLIP_PLANES	5

// The basic solid body movement clip that slides along multiple planes
void Bumper::try_move(const btDynamicsWorld *world, const glm::vec3 &velocity, float delta)
{
	bool blocked = false; // assume not blocked
	int numplanes = 0; //  and not sliding along any planes
	glm::vec3 planes[MAX_CLIP_PLANES];

	glm::vec3 mv_velocity = velocity;

	// Store original velocity
	glm::vec3 original_velocity = velocity;
	glm::vec3 primal_velocity = velocity;

	float time_left = delta;   // Total time for this movement operation.

	int i = 0;
	int j = 0;

	glm::vec3 mv_origin = bt_to_vec3(ghost_object->getWorldTransform().getOrigin());

	bool no_hit = false;

	int numbumps = 4;
	int bumpcount = 0;
	for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
		if (glm::length(mv_velocity) == 0.f) {
			break;
		}
		// Assume we can move all the way from the current origin to the end point
		glm::vec3 end = mv_origin + time_left * mv_velocity;

		Trace trace = trace_movement(ghost_object.get(), shape.get(), world, mv_origin, end);

		if (trace.fraction > 0.f) {	// actually covered some distance
			mv_origin = trace.endpos;
			numplanes = 0;
		}

		if (trace.fraction == 1.f) {
			no_hit = true;
			break;		// moved the entire distance
		}

		// TODO check if floor or step
		/*
		if (trace.plane.normal.y > 0.7f) {
			blocked |= 1;		// floor
		}
		if (trace.plane.normal.y == 0.f) {
			blocked |= 2;		// step
		}
		*/

		// adjust time left
		time_left -= time_left * trace.fraction;

		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES) {	// this shouldn't really happen
			mv_velocity = glm::vec3(0.f);
			break;
		}

		planes[numplanes] = trace.normal;
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++) {
			clip_velocity(original_velocity, planes[i], mv_velocity, 1);
			for (j = 0; j < numplanes; j++) {
				if (j != i) {
					if (glm::dot(mv_velocity, planes[j]) < 0.f) {
						break;	// not ok
					}
				}
			}
			if (j == numplanes) {
				break;
			}
		}

		if (i != numplanes) {	// go along this plane
		} else {	// go along the crease
			if (numplanes != 2) {
				mv_velocity = glm::vec3(0.f);
				break;
			}
			glm::vec3 dir = glm::cross(planes[0], planes[1]);
			float d = glm::dot(dir, mv_velocity);
			mv_velocity = d * dir;
		}

		// if velocity is against the original velocity, stop dead
// to avoid tiny occilations in sloping corners
		if (glm::dot(mv_velocity, primal_velocity) <= 0.f) {
			mv_velocity = glm::vec3(0.f);
			break;
		}
	}
	
	if (no_hit) {
		teleport(mv_origin);
	} else {
		apply_velocity(delta * mv_velocity);
	}
}

Trace trace_movement(btPairCachingGhostObject *object, const btConvexShape *shape, const btDynamicsWorld *world, const glm::vec3 &origin, const glm::vec3 &destination)
{
	Trace trace = {};
	trace.fraction = 1.f;
	trace.endpos = destination;

	glm::vec3 current_position = bt_to_vec3(object->getWorldTransform().getOrigin());

	glm::vec3 sweep_direction = current_position - destination;

	btTransform start = object->getWorldTransform();
	btTransform end = start;
	end.setOrigin(vec3_to_bt(destination));

	BumperConvexCallback callback(object, vec3_to_bt(sweep_direction), btScalar(0.0));
	callback.m_collisionFilterGroup = object->getBroadphaseHandle()->m_collisionFilterGroup;
	callback.m_collisionFilterMask = object->getBroadphaseHandle()->m_collisionFilterMask;

	object->convexSweepTest(shape, start, end, callback, world->getDispatchInfo().m_allowedCcdPenetration);

	glm::vec3 direction = destination - current_position;
	float distance = glm::length(direction);

	if (callback.hasHit() && object->hasContactResponse()) {
		trace.fraction = callback.m_closestHitFraction;
		trace.normal = bt_to_vec3(callback.m_hitNormalWorld);
		// TODO this is incorrect
		trace.endpos = current_position + (0.5f * trace.fraction * direction);
	}

	return trace;
}

};

