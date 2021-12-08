#pragma once
#include "../extern/bullet/btBulletDynamicsCommon.h"
#include "../extern/bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "../extern/bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

namespace fysx {

struct RayResult {
	bool hit = false;
	glm::vec3 point = {};
	const btCollisionObject *object = nullptr; // the body if ray hit
};

class CollisionMesh {
public:
	std::unique_ptr<btTriangleMesh> mesh;
	std::unique_ptr<btCollisionShape> shape;
public:
	CollisionMesh(const std::vector<glm::vec3> &positions, const std::vector<uint16_t> &indices);
};

class PhysicalSystem {
public:
	PhysicalSystem();
	~PhysicalSystem();
public:
	void update(float delta);
	void update_collision_only();
public:
	void add_body(btRigidBody *body, int group = btBroadphaseProxy::DefaultFilter, int mask = btBroadphaseProxy::AllFilter);
	void remove_body(btRigidBody *body);
	void add_object(btCollisionObject *object, int group = btBroadphaseProxy::DefaultFilter, int mask = btBroadphaseProxy::AllFilter);
	void remove_object(btCollisionObject *object);
	void clear_objects();
public:
	RayResult cast_ray(const glm::vec3 &origin, const glm::vec3 &end, int mask = btBroadphaseProxy::AllFilter) const; 
public:
	const btDynamicsWorld* world() const;
	btDynamicsWorld* world();
private:
	std::unique_ptr<btCollisionConfiguration> m_config;
	std::unique_ptr<btCollisionDispatcher> m_dispatcher;
	std::unique_ptr<btBroadphaseInterface> m_broadphase;
	std::unique_ptr<btConstraintSolver> m_solver;
	std::unique_ptr<btDynamicsWorld> m_world;
	std::unique_ptr<btGhostPairCallback> m_ghost_callback;
};

inline glm::vec3 bt_to_vec3(const btVector3 &v)
{
	return glm::vec3(v.x(), v.y(), v.z());
}

// constructor order for glm quaternion is (w, x, y, z)
inline glm::quat bt_to_quat(const btQuaternion &q)
{
	return glm::quat(q.w(), q.x(), q.y(), q.z());
}

inline btQuaternion quat_to_bt(const glm::quat &q)
{
	return btQuaternion(q.x, q.y, q.z, q.w);
}

inline btVector3 vec3_to_bt(const glm::vec3 &v)
{
	return btVector3(v.x, v.y, v.z);
}

inline glm::vec3 body_position(const btRigidBody *body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);

	return glm::vec3(float(t.getOrigin().x()), float(t.getOrigin().y()), float(t.getOrigin().z()));
}

inline glm::quat body_rotation(const btRigidBody *body)
{
	btTransform t;
	body->getMotionState()->getWorldTransform(t);

	return glm::quat(float(t.getRotation().w()), float(t.getRotation().x()), float(t.getRotation().y()), float(t.getRotation().z()));
}

};
