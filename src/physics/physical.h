#pragma once
#include "../extern/bullet/btBulletDynamicsCommon.h"
#include "../extern/bullet/BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "../extern/bullet/BulletCollision/CollisionDispatch/btGhostObject.h"

namespace fysx {

struct RayResult {
	bool hit = false;
	glm::vec3 point = {};
};

class PhysicalSystem {
public:
	PhysicalSystem();
public:
	void update(float delta);
	void add_body(btRigidBody *body);
	void remove_body(btRigidBody *body);
	void clear_objects();
public:
	RayResult cast_ray(const glm::vec3 &origin, const glm::vec3 &end) const; 
private:
	std::unique_ptr<btCollisionConfiguration> m_config;
	std::unique_ptr<btCollisionDispatcher> m_dispatcher;
	std::unique_ptr<btBroadphaseInterface> m_broadphase;
	std::unique_ptr<btConstraintSolver> m_solver;
	std::unique_ptr<btDynamicsWorld> m_world;
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

};
