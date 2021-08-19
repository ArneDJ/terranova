namespace fysx {

class PlaneField {
public:
	PlaneField()
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

	}
public:
	btRigidBody* body()
	{
		return m_body.get();
	}
private:
	std::unique_ptr<btDefaultMotionState> m_motion;
	std::unique_ptr<btCollisionShape> m_shape;
	std::unique_ptr<btRigidBody> m_body;
};

class HeightField {
public:
	HeightField(const util::Image<float> &image, const glm::vec3 &scale);
	HeightField(const util::Image<uint8_t> &image, const glm::vec3 &scale);
public:
	btCollisionObject *object();
	const btCollisionObject *object() const;
private:
	std::unique_ptr<btHeightfieldTerrainShape> m_shape;
	std::unique_ptr<btCollisionObject> m_object;
};


};
