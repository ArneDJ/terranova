namespace fysx {

class HeightField {
public:
	HeightField()
	{
		m_shape = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 1.f, 0.f), 0.f);

		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(btVector3(0, 0, 0));

		btScalar mass(0.);

		btVector3 inertia(0, 0, 0);

		//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
		m_motion = std::make_unique<btDefaultMotionState>(transform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, m_motion.get(), m_shape.get(), inertia);
		m_body = std::make_unique<btRigidBody>(rbInfo);

		m_transform = std::make_unique<geom::Transform>();
		m_transform->position = glm::vec3(0.f, 0.f, 0.f);
	}
public:
	btRigidBody* body()
	{
		return m_body.get();
	}
public:
	const geom::Transform* transform()
	{
		return m_transform.get();
	}
private:
	std::unique_ptr<btDefaultMotionState> m_motion;
	std::unique_ptr<btCollisionShape> m_shape;
	std::unique_ptr<btRigidBody> m_body;
private:
	std::unique_ptr<geom::Transform> m_transform;
};

};
